/*
 *  Copyright (c) 2008-2017 SLIBIO. All Rights Reserved.
 *
 *  This file is part of the SLib.io project.
 *
 *  This Source Code Form is subject to the terms of the Mozilla Public
 *  License, v. 2.0. If a copy of the MPL was not distributed with this
 *  file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */


#include "../../../inc/slibx/sfile/secure_file_pack.h"

#include <slib/core/log.h>
#include <slib/crypto/sha2.h>
#include <slib/crypto/aes.h>

namespace slib
{

	#define FILE_PACKAGE_SIGNATURE 0x4B504C53   /*SLPK*/

	struct _SECURE_FILE_PACKAGE_HEADER {
		sl_uint32 signature;  /*SLPK*/
		sl_uint16 type; // reserved
		sl_uint16 version; // reserved
		sl_uint8 password_hash[32];  // SHA-256 ^ 2
		sl_uint8 password_empty;
		sl_uint8 reserved[23]; // for 16 byte align
	}; // 64 byte

	struct _SECURE_FILE_PACKAGE_INDEX_HEADER {
		sl_uint64 nFilesTotal; // include directories
		sl_uint64 nDirectories;
		sl_uint64 nFiles;
		sl_uint64 nFilesEmpty;
		sl_uint8 reserved[32]; // for 16 byte align
	}; // 64 byte

	#define LEN_FILE_NAME 96
	struct _SECURE_FILE_PACKAGE_INDEX {
		sl_char16 fileName[LEN_FILE_NAME]; // clamped
		sl_uint32 type; // 0: empty, 1: directory,  2: file
		sl_uint32 reserved;
		sl_uint64 position;
		sl_uint64 size;
		sl_uint64 timeModified;
	}; // 128 byte

	struct _SECURE_FILE_CONTENT_HEADER {
		sl_uint64 size;
		sl_uint32 sizeHeader;
		sl_uint32 lengthFileName;
	};

	sl_bool SecureFilePackage::create(String filePath, const CreateParam& param, String pathSourceDirectory, Progress* progress)
	{
		if (File::isDirectory(pathSourceDirectory)) {
			List<String> list = File::getAllDescendantFiles(pathSourceDirectory);
			return createFromFiles(filePath, param, pathSourceDirectory + "/", list, sl_true, progress);
		} else {
			return sl_false;
		}
	}

	sl_bool SecureFilePackage::createFromFiles(String filePath, const CreateParam& param, String pathSourceBase, List<String> listSourceFilePaths, sl_bool flagSorted, Progress* progress)
	{
	#define TAG "SecureFilePackage - create"
		
		Ref<File> file = File::openForWrite(filePath);

		if (file.isNull()) {
			LogError(TAG, "Can not open target file (%s)", filePath);
			return sl_false;
		}

		if (!flagSorted) {
			listSourceFilePaths.sort(sl_true);
		}

		sl_uint8 password[32];
		String strPassword = param.password;
		sl_bool flagPassword = sl_false;
		if (strPassword.getLength() > 0) {
			SHA256::hash(strPassword, password);
			flagPassword = sl_true;
		}

		_SECURE_FILE_PACKAGE_HEADER header;
		Base::resetMemory(&header, 0, sizeof(header));
		header.signature = FILE_PACKAGE_SIGNATURE;
		header.type = 0;
		header.version = 0;
		header.password_empty = 1;
		if (flagPassword) {
			header.password_empty = 0;
			SHA256::hash(password, 32, header.password_hash);
		}
		if (file->write(&header, sizeof(header)) != sizeof(header)) {
			LogError(TAG, "Writing header error");
			file->close();
			return sl_false;
		}

		_SECURE_FILE_PACKAGE_INDEX_HEADER indexHeader;

		Math::randomMemory(&indexHeader, sizeof(indexHeader));
		indexHeader.nDirectories = 0;
		indexHeader.nFilesEmpty = 0;
		indexHeader.nFiles = 0;
		indexHeader.nFilesTotal = 0;

		sl_int64 posIndexHeader = file->getPosition();
		if (file->write(&indexHeader, sizeof(indexHeader)) != sizeof(indexHeader)) {
			LogError(TAG, "Writing index header error");
			file->close();
			return sl_false;
		}

		sl_size nFiles = listSourceFilePaths.getCount();
		if (nFiles > (sl_uint32)(-1) / sizeof(_SECURE_FILE_PACKAGE_INDEX) / 4) {
			LogError(TAG, "Maximum file count reached");
			file->close();
			return sl_false;
		}
		indexHeader.nFilesTotal = nFiles;
		sl_size sizeIndex = sizeof(_SECURE_FILE_PACKAGE_INDEX)* nFiles;
		_SECURE_FILE_PACKAGE_INDEX* indexes = (_SECURE_FILE_PACKAGE_INDEX*)(Base::createMemory(sizeIndex));
		Math::randomMemory(indexes, sizeIndex);
		if (!indexes) {
			LogError(TAG, "Index creation - out of memory");
			file->close();
			return sl_false;
		}
		sl_int64 posIndexes = file->getPosition();
		if (file->write(indexes, sizeIndex) != sizeIndex) {
			LogError(TAG, "Write index table failed");
			Base::freeMemory(indexes);
			file->close();
			return sl_false;
		}

		AES enc;
		enc.setKey(password, 32);
	#define FILE_READ_BUFFER_SIZE (1024*256)
		sl_uint8* block = (sl_uint8*)(Base::createMemory(FILE_READ_BUFFER_SIZE));
		for (sl_size i = 0; i < nFiles; i++) {
			String fileName = (listSourceFilePaths.getData())[i];
			String path = pathSourceBase + fileName;

			indexes[i].type = indexTypeEmpty;
			indexes[i].fileName[0] = 0;
			indexes[i].position = 0;
			indexes[i].size = 0;
			indexes[i].timeModified = 0;

			if (progress) {
				if (progress->flagRequestStop) {
					Base::freeMemory(indexes);
					Base::freeMemory(block);
					file->close();
					return sl_false;
				}
				progress->lastFilePath = path;
			}
			if (File::exists(path)) {
				String16 fileName16 = fileName;
				sl_uint32 lenFileName = (sl_uint32)(fileName16.getLength());
				if (lenFileName > LEN_FILE_NAME-1) {
					lenFileName = LEN_FILE_NAME-1;
					Log(TAG, "Filename length over - %s", fileName);
				}
				Base::copyMemory(indexes[i].fileName, fileName16.getData(), lenFileName * sizeof(sl_char16));
				indexes[i].fileName[lenFileName] = 0;

				if (File::isDirectory(path)) {
					indexes[i].type = indexTypeDirectory;
					indexHeader.nDirectories++;
				} else {
					Ref<File> fileSource = File::openForRead(path);
					if (fileSource.isNotNull()) {
						indexHeader.nFiles++;
						if (progress) {
							progress->nFiles++;
						}

						indexes[i].type = indexTypeFile;
						sl_int64 sizeFile = fileSource->getSize();
						indexes[i].size = sizeFile;
						indexes[i].timeModified = File::getModifiedTime(path).getMillisecondsCount();
						indexes[i].position = file->getPosition();

						int sizeContentHeader = ((((sizeof(_SECURE_FILE_CONTENT_HEADER)+sizeof(sl_char16)*(sl_uint32)(fileName.getLength())) - 1) | 15) + 1);
						_SECURE_FILE_CONTENT_HEADER* contentHeader = (_SECURE_FILE_CONTENT_HEADER*)Base::createMemory(sizeContentHeader);
						Math::randomMemory(contentHeader, sizeContentHeader);
						contentHeader->size = sizeContentHeader + sizeFile;
						contentHeader->sizeHeader = sizeContentHeader;
						contentHeader->lengthFileName = (sl_uint32)(fileName.getLength());
						Base::copyMemory(contentHeader + 1, fileName16.getData(), fileName16.getLength() * sizeof(sl_char16));
						if (flagPassword) {
							enc.encryptBlocks(contentHeader, contentHeader, sizeContentHeader);
						}

						if (file->write(contentHeader, sizeContentHeader) != sizeContentHeader) {
							LogError(TAG, "Write content header failed");
							Base::freeMemory(contentHeader);
							Base::freeMemory(indexes);
							Base::freeMemory(block);
							fileSource->close();
							file->close();
							return sl_false;
						}
						Base::freeMemory(contentHeader);
						sl_int64 sizeFileSum = 0;
						while (1) {
							sl_size r = fileSource->read(block, FILE_READ_BUFFER_SIZE);
							if (r <= 0) {
								break;
							}
							sl_size r16 = (((r - 1) | 15) + 1);
							Math::randomMemory(block + r, r16 - r);
							if (flagPassword) {
								enc.encryptBlocks(block, block, r16);
							}
							if (file->write(block, r16) != r16) {
								LogError(TAG, "Write content failed");
								Base::freeMemory(indexes);
								Base::freeMemory(block);
								fileSource->close();
								file->close();
								return sl_false;
							}
							sizeFileSum += r16;

							if (progress) {
								if (progress->flagRequestStop) {
									Base::freeMemory(indexes);
									Base::freeMemory(block);
									fileSource->close();
									file->close();
									return sl_false;
								}
								progress->nSize += r;
							}
							if (r < FILE_READ_BUFFER_SIZE) {
								break;
							}
						}
						if (sizeFileSum != (((sizeFile - 1) | 15) + 1)) {
							Log(TAG, "Size mismatch - %s", path);
						}
						fileSource->close();
					} else {
						Log(TAG, "File open failed - %s", path);
						indexes[i].type = indexTypeEmpty;
						indexHeader.nFilesEmpty++;
					}
				}
			} else {
				Log(TAG, "File does not exist - %s", path);
				indexes[i].type = indexTypeEmpty;
				indexHeader.nFilesEmpty++;
			}
		}
		Base::freeMemory(block);

		file->seek(posIndexHeader, SeekPosition::Begin);
		if (flagPassword) {
			enc.encryptBlocks(&indexHeader, &indexHeader, sizeof(indexHeader));
		}
		if (file->write(&indexHeader, sizeof(indexHeader)) != sizeof(indexHeader)) {
			LogError(TAG, "Write encrypted index header failed");
			Base::freeMemory(indexes);
			file->close();
			return sl_false;
		}

		file->seek(posIndexes, SeekPosition::Begin);
		if (flagPassword) {
			enc.encryptBlocks(indexes, indexes, sizeIndex);
		}
		if (file->write(indexes, sizeIndex) != sizeIndex) {
			LogError(TAG, "Write encrypted index table failed");
			Base::freeMemory(indexes);
			file->close();
			return sl_false;
		}
		Base::freeMemory(indexes);
		file->close();
		return sl_true;
	#undef TAG
	}

	SecureFilePackage::ErrorCode SecureFilePackage::open(String filePath, const OpenParam& param)
	{
		Ref<File> file = File::openForRead(filePath);
		if (file.isNull()) {
			return errorNotOpened;
		}
		String strPassword = param.password;
		sl_bool flagPassword = sl_false;
		sl_uint8 password[32];
		if (strPassword.getLength() > 0) {
			flagPassword = sl_true;
			SHA256::hash(strPassword, password);
		}

		_SECURE_FILE_PACKAGE_HEADER header;
		if (file->read(&header, sizeof(header)) != sizeof(header)) {
			file->close();
			return errorInvalidHeader;
		}

		if (header.signature != FILE_PACKAGE_SIGNATURE) {
			file->close();
			return errorInvalidHeader;
		}

		if (flagPassword) {
			if (header.password_empty) {
				file->close();
				return errorPasswordMismatch;
			}
			sl_uint8 password_hash[32];
			SHA256::hash(password, 32, password_hash);
			if (Base::compareMemory(password_hash, header.password_hash, 32) != 0) {
				file->close();
				return errorPasswordMismatch;
			}
		} else {
			if (!header.password_empty) {
				file->close();
				return errorPasswordMismatch;
			}
		}

		m_filePath = filePath;
		Base::copyMemory(m_password, password, 32);
		m_flagPassword = flagPassword;

		file->close();
		return errorOK;
	}

	List<SecureFilePackage::FileDesc> SecureFilePackage::getFiles()
	{
		List<FileDesc> list;

	#define TAG "SecureFilePackage - getFiles"

		if (m_filePath.getLength() <= 0) {
			return list;
		}

		Ref<File> file = File::openForRead(m_filePath);
		if (file.isNull()) {
			LogError(TAG, "can not open file - %s", m_filePath);
			return list;
		}

		file->seek(sizeof(_SECURE_FILE_PACKAGE_HEADER), SeekPosition::Begin);

		_SECURE_FILE_PACKAGE_INDEX_HEADER indexHeader;
		if (file->read(&indexHeader, sizeof(indexHeader)) != sizeof(indexHeader)) {
			LogError(TAG, "index header read error - %s", m_filePath);
			file->close();
			return list;
		}

		AES dec;
		dec.setKey(m_password, 32);

		if (m_flagPassword) {
			dec.decryptBlocks(&indexHeader, &indexHeader, sizeof(indexHeader));
		}

		sl_uint32 n = (sl_uint32)(indexHeader.nFilesTotal);
		for (sl_uint32 i = 0; i < n; i++) {
			_SECURE_FILE_PACKAGE_INDEX index;
			if (file->read(&index, sizeof(index)) != sizeof(index)) {
				Log(TAG, "index %d read error - %s", i, m_filePath);
				break;
			} else {
				if (m_flagPassword) {
					dec.decryptBlocks(&index, &index, sizeof(index));
				}
				FileDesc desc;
				desc.filePath = index.fileName;
				desc.type = (IndexType)(index.type);
				desc.position = index.position;
				desc.size = index.size;
				desc.timeModified = index.timeModified;
				list.add(desc);
			}
		}

		file->close();
		return list;

	#undef TAG
	}

	// find file and returns the location
	sl_bool SecureFilePackage::findFile(String fileName, FileDesc* output)
	{

	#define TAG "SecureFilePackage - findFile"

		if (m_filePath.getLength() <= 0) {
			return sl_false;
		}

		Ref<File> file = File::openForRead(m_filePath);
		if (file.isNull()) {
			LogError(TAG, "can not open file - %s", m_filePath);
			return sl_false;
		}

		file->seek(sizeof(_SECURE_FILE_PACKAGE_HEADER), SeekPosition::Begin);

		_SECURE_FILE_PACKAGE_INDEX_HEADER indexHeader;
		if (file->read(&indexHeader, sizeof(indexHeader)) != sizeof(indexHeader)) {
			LogError(TAG, "index header read error - %s", m_filePath);
			file->close();
			return sl_false;
		}

		AES dec;
		dec.setKey(m_password, 32);

		if (m_flagPassword) {
			dec.decryptBlocks(&indexHeader, &indexHeader, sizeof(indexHeader));
		}

		int n = (int)(indexHeader.nFilesTotal);
		if (n <= 0) {
			Log(TAG, "index is empty - %s", m_filePath);
			file->close();
			return sl_false;
		}
		int start = 0;
		int end = n - 1;
		sl_int64 pre = file->getPosition();
		
		fileName = fileName.replaceAll("/", "\t");
		fileName = fileName.replaceAll("\\", "\t");
		while (1) {
			int mid = (start + end) / 2;

			_SECURE_FILE_PACKAGE_INDEX index;
			file->seek(pre + mid * sizeof(_SECURE_FILE_PACKAGE_INDEX), SeekPosition::Begin);
			if (file->read(&index, sizeof(index)) != sizeof(index)) {
				Log(TAG, "index %d read error - %s", mid, m_filePath);
				break;
			} else {
				if (m_flagPassword) {
					dec.decryptBlocks(&index, &index, sizeof(index));
				}
				String sp = index.fileName;
				sp = sp.replaceAll("/", "\t");
				int cmp = fileName.compare(sp);
				if (cmp == 0) {
					if (output) {
						FileDesc& desc = *output;
						desc.filePath = index.fileName;
						desc.type = (IndexType)(index.type);
						desc.position = index.position;
						desc.size = index.size;
						desc.timeModified = index.timeModified;
					}

					file->close();
					return sl_true;
				}
				if (start == end) {
					break;
				}
				if (cmp > 0) {
					if (start == mid) {
						start = mid + 1;
					} else {
						start = mid;
					}
				} else {
					end = mid;
				}
			}
		}

		file->close();
		return sl_false;

	#undef TAG
	}

	Memory SecureFilePackage::readFile(sl_int64 position, String* pFileName)
	{
		Memory ret;
	#define TAG "SecureFilePackage - readFile"

		if (m_filePath.getLength() <= 0) {
			return ret;
		}

		Ref<File> file = File::openForRead(m_filePath);
		if (file.isNull()) {
			LogError(TAG, "can not open file - %s", m_filePath);
			return ret;
		}

		AES dec;
		dec.setKey(m_password, 32);

		file->seek(position, SeekPosition::Begin);
		_SECURE_FILE_CONTENT_HEADER contentHeader;
		if (file->read(&contentHeader, sizeof(contentHeader)) != sizeof(contentHeader)) {
			file->close();
			return ret;
		}

		if (m_flagPassword) {
			dec.decryptBlocks(&contentHeader, &contentHeader, sizeof(_SECURE_FILE_CONTENT_HEADER));
		}
		if (pFileName) {
			sl_int32 n = contentHeader.sizeHeader - sizeof(_SECURE_FILE_CONTENT_HEADER);
			if ((sl_int32)(contentHeader.lengthFileName * sizeof(sl_char16)) > n) {
				LogError(TAG, "filename length out on block %d - %s", position, m_filePath);
				file->close();
				return ret;
			}
			sl_uint8* buf = (sl_uint8*)(Base::createMemory(n));
			if (buf) {
				Base::resetMemory(buf, 0, n);
				if (file->read(buf, n) != n) {
					LogError(TAG, "read filename failed on block %d - %s", position, m_filePath);
					Base::freeMemory(buf);
					file->close();
					return ret;
				}
				if (m_flagPassword) {
					dec.decryptBlocks(buf, buf, n);
				}
				String str((sl_char16*)buf, contentHeader.lengthFileName);
				*pFileName = str;
				Base::freeMemory(buf);
			} else {
				LogError(TAG, "out of memory on filename on block %d - %s", position, m_filePath);
				file->close();
				return ret;
			}
		} else {
			file->seek(contentHeader.sizeHeader - sizeof(_SECURE_FILE_CONTENT_HEADER), SeekPosition::Current);
		}

		int fileSize = (int)(contentHeader.size - contentHeader.sizeHeader);
		ret = Memory::create(fileSize);
		if (ret.isEmpty()) {
			file->close();
			return ret;
		}

		sl_uint8* bufRet = (sl_uint8*)(ret.getData());
		sl_uint8* block = (sl_uint8*)Base::createMemory(FILE_READ_BUFFER_SIZE);
		int nRead = 0;
		while (nRead < fileSize) {
			int m = fileSize - nRead;
			if (m > FILE_READ_BUFFER_SIZE) {
				m = FILE_READ_BUFFER_SIZE;
			}
			int mm = ((m - 1) | 15) + 1;
			if (file->read(block, mm) != mm) {
				break;
			}
			if (m_flagPassword) {
				dec.decryptBlocks(block, block, mm);
			}
			Base::copyMemory(bufRet + nRead, block, m);
			nRead += m;
		}
		if (nRead != fileSize) {
			Log(TAG, "file size mismatch the content on block %d - %s", position, m_filePath);
		}

		Base::freeMemory(block);
		file->close();
		return ret;

	#undef TAG
	}

	sl_bool SecureFilePackage::extract(String pathTargetDirectory, Progress* progress)
	{
		if (pathTargetDirectory.getLength() <= 0) {
			return sl_false;
		}
		ListLocker<FileDesc> list(getFiles());

		sl_size n = list.count;
		for (sl_size i = 0; i < n; i++) {
			FileDesc& desc = list[i];
			if (desc.type == indexTypeFile) {
				String filePath;
				Memory memory = readFile(desc.position, &filePath);
				if (memory.isNotEmpty()) {
					String path = pathTargetDirectory + "/" + filePath;
					File::createDirectories(File::getParentDirectoryPath(path));
					File::writeAllBytes(path, memory.getData(), memory.getSize());
					File::setModifiedTime(path, desc.timeModified);
					if (progress) {
						if (progress->flagRequestStop) {
							return sl_false;
						}
						progress->lastFilePath = path;
						progress->nFiles++;
						progress->nSize += memory.getSize();
					}
				}
			}
		}

		return sl_true;
	}

}
