#include "package.h"
#include "../../slib/core/io.h"

#define PACKAGE_ITEM_IDENTIFY	0xFEFF0823
#define PACKAGE_HEADER_SIZE		32
#define PACKAGE_IDENTIFY		_SLT("SMAP-PACKAGE V1.0")

SLIB_MAP_NAMESPACE_START

void MapPackage::create(const String& filePath)
{
	m_pkgFile->setPath(filePath);
	String dirPath = File::getParentDirectoryPath(m_pkgFile->getPath());
	File dir(dirPath);
	dir.createDirectories();
	if (m_pkgFile->openForWrite()) {
		Memory header = getHeader();
		sl_int32 offsetTableSize = sizeof(sl_int32)* m_nTilesXNum * m_nTilesYNum;
		SLIB_SCOPED_ARRAY(sl_int32, offsetTable, offsetTableSize);
		Base::zeroMemory(offsetTable, offsetTableSize * sizeof(sl_int32));
		m_pkgFile->write(header.getBuf(), header.getSize());
		m_pkgFile->write(offsetTable, offsetTableSize);
		m_pkgFile->seekToBegin();
		m_pkgFile->close();
	}
}

Memory MapPackage::getHeader()
{
	Memory ret = Memory::create(PACKAGE_HEADER_SIZE);
	MemoryWriter writer(ret);

	Base::zeroMemory(ret.getBuf(), ret.getSize());
	String packageIdentify = PACKAGE_IDENTIFY;
	Utf8StringBuffer buffer = packageIdentify.getUtf8();
	writer.write(buffer.sz, buffer.len);
	writer.writeInt32CVLI(m_nTilesXNum);
	writer.writeInt32CVLI(m_nTilesYNum);
	return ret;
}

sl_bool MapPackage::checkHeader()
{
	sl_bool ret = sl_true;
	Memory originalHeader = getHeader();
	Memory header = Memory::create(PACKAGE_HEADER_SIZE);
	m_pkgFile->read(header.getBuf(), header.getSize());
	if (Base::compareMemory(originalHeader.getBuf(), header.getBuf(), PACKAGE_HEADER_SIZE) != 0) {
		ret = sl_false;
	}
	return ret;
}

sl_bool MapPackage::open(const String& filePath, sl_bool flagReadOnly)
{
	close();
	m_pkgFile = new File();
	if (!File::exists(filePath) && !flagReadOnly) {
		create(filePath);
		m_pkgFile->close();
	}
	sl_bool flagOpen = m_pkgFile->open(filePath, File::modeReadWriteNoTruncate);
	if (flagOpen && checkHeader()) {
		m_flagOpen = sl_true;
		return sl_true;
	}
	return sl_false;
}

sl_int32 MapPackage::getTableOffset(sl_int32 offsetX, sl_int32 offsetY)
{
	sl_int32 ret = PACKAGE_HEADER_SIZE + (offsetY * m_nTilesXNum + offsetX) * sizeof(sl_int32);
	return ret;
}

sl_int32 MapPackage::getItemOffset(sl_int32 x, sl_int32 y)
{
	sl_int32 ret = -1;
	sl_int32 offsetToTable = getTableOffset(x, y);
	if (offsetToTable + 4 < m_pkgFile->getSize()) {
		m_pkgFile->seek(offsetToTable, File::positionBegin);
		m_pkgFile->read(&ret, sizeof(sl_int32));
		m_pkgFile->seekToBegin();
	}
	return ret;
}

Memory MapPackage::getItem(sl_int32 itemOffset)
{
	Memory ret;
	if (itemOffset + 4 < m_pkgFile->getSize()) {
		m_pkgFile->seek(itemOffset);
		sl_int32 identify = 0;
		m_pkgFile->read(&identify, sizeof(sl_int32));
		if (identify == PACKAGE_ITEM_IDENTIFY) {
			sl_int32 itemSize = 0;
			m_pkgFile->read(&itemSize, sizeof(sl_int32));
			ret = Memory::create(itemSize);
			m_pkgFile->read(ret.getBuf(), itemSize);
		}
	}
	return ret;
}

static SLIB_INLINE void writeString(MemoryWriter& writer, const String& str) {
	Utf8StringBuffer buffer = str.getUtf8();
	writer.writeUint32CVLI(buffer.len);
	if (buffer.len > 0) {
		writer.write(buffer.sz, buffer.len);
	}
}

static SLIB_INLINE String readString(MemoryReader& reader)
{
	sl_uint32 nameLen = reader.readUint32CVLI();
	String ret = _SLT("");
	if (nameLen > 0){
		slib::ScopedArray<sl_uint8> strName(nameLen);
		reader.read(strName + 0, nameLen);
		ret = String::fromUtf8((sl_str8)(strName + 0), nameLen);
	}
	return ret;
}

static inline void writeItemData(MemoryWriter& writer, const String& key, const Memory& data)
{
	writeString(writer, key);
	writer.writeInt32CVLI(data.getSize());
	writer.write(data.getBuf(), data.getSize());
}

static inline Memory readItemData(MemoryReader& reader, String& key)
{
	Memory ret;
	key = readString(reader);
	sl_int32 itemSize = reader.readInt32CVLI();
	if (itemSize > 0) {
		ret = Memory(itemSize);
		reader.read(ret.getBuf(), itemSize);
	}
	
	return ret;
}

Map<String, Memory> MapPackage::getDataFromItem(const Memory& encData)
{
	Map<String, Memory> ret;
	Memory item;
	if (encData.isNotEmpty()) {
		Memory decData = Memory::create(encData.getSize() + 256);
		sl_size decSize = m_encryption.decrypt_CBC_PKCS7Padding(encData.getBuf(), encData.getSize(), decData.getBuf());
		item = Memory(decData.getBuf(), decSize);
	}
	if (item.isNotEmpty()) {
		MemoryReader reader(item);
		sl_int64 itemUpdateTime = reader.readInt64CVLI();
		sl_int32 itemNextOffset = reader.readInt32CVLI();
		sl_int32 itemCount = reader.readInt32CVLI();
		for (sl_int32 i = 0; i < itemCount; i++) {
			String key = _SLT("");
			Memory itemData = readItemData(reader, key);
			if (itemData.isNotEmpty()) {
				ret.put(key, itemData);
			}
		}
	}
	return ret;
}

Memory MapPackage::createItem(const Map<String, Memory>& itemData, sl_int32 oldItemOffset)
{
	MemoryWriter writer;

	sl_int64 curTime = Time::now().getSecondsCount();

	writer.writeInt64CVLI(curTime);
	writer.writeInt32CVLI(oldItemOffset);
	
	sl_int32 itemCount = itemData.getCount();
	writer.writeInt32CVLI(itemCount);

	auto itemIter = itemData.iterator();
	Pair<String, Memory> pairValue;
	while (itemIter.next(&pairValue)) {
		writeItemData(writer, pairValue.key, pairValue.value);
	}
	
	Memory item = writer.getData();

	Memory encryption = Memory::create(item.getSize() + 256);
	sl_size encryptSize = m_encryption.encrypt_CBC_PKCS7Padding(item.getBuf(), item.getSize(), encryption.getBuf());

	Memory ret(encryption.getBuf(), encryptSize);
	return ret;
}

sl_bool MapPackage::write(sl_int32 offsetX, sl_int32 offsetY, const Map<String, Memory>& itemData)
{
	sl_bool ret = sl_false;
	if (m_flagOpen && itemData.getCount() > 0) {
		Memory packageItem = createItem(itemData, 0);
		sl_int32 itemPosition = (sl_int32)m_pkgFile->getSize();
		sl_int32 tblOffset = getTableOffset(offsetX, offsetY);

		m_pkgFile->seek(tblOffset, File::positionBegin);
		sl_int32 nRet = m_pkgFile->write(&itemPosition, sizeof(sl_int32));
		if (nRet != 4) {
			return sl_false;
		}

		m_pkgFile->seekToEnd();

		sl_int32 itemIdentify = PACKAGE_ITEM_IDENTIFY;
		nRet = m_pkgFile->write(&itemIdentify, sizeof(itemIdentify));
		if (nRet != sizeof(itemIdentify)) {
			return sl_false;
		}

		sl_int32 packageSize = packageItem.getSize();
		nRet = m_pkgFile->write(&packageSize, sizeof(packageSize));
		if (nRet != sizeof(packageSize)) {
			return sl_false;
		}

		nRet = m_pkgFile->write(packageItem.getBuf(), packageSize);
		if (nRet != packageItem.getSize()) {
			return sl_false;
		}
		ret = sl_true;
	}
	return ret;
}

Memory MapPackage::read(sl_int32 offsetX, sl_int32 offsetY, const String& subName)
{
	Memory ret;
	sl_int32 currentVersion = 0;
	sl_int32 itemOffset = getItemOffset(offsetX, offsetY);
	if (itemOffset != -1) {
		Map<String, Memory> items = getDataFromItem(getItem(itemOffset));
		Memory defaultItemValue;
		ret = items.getValue(subName, defaultItemValue);
	}
	return ret;
}

Memory MapPackage::read(const String& dirPath, const MapTileLocation& loc, const String& subName)
{	
	sl_int32 offsetX, offsetY;
	String path = dirPath + _SLT("/") + getPackageFilePathAndOffset(loc, offsetX, offsetY);
	Memory ret;
	if (open(path, sl_true)) {
		ret = read(offsetX, offsetY, subName);
	}
	close();
	return ret;
}

sl_bool MapPackage::write(const String& dirPath, const MapTileLocation& loc, const Map<String, Memory>& itemData)
{
	sl_int32 offsetX, offsetY;
	String path = dirPath + _SLT("/") +getPackageFilePathAndOffset(loc, offsetX, offsetY);
	sl_bool ret = sl_false;
	if (open(path, sl_false)) {
		ret = write(offsetX, offsetY, itemData);
	}
	close();
	return ret;
}

String MapPackage::getPackageFilePathAndOffset(const MapTileLocation& location, sl_int32& outX, sl_int32& outY)
{
	String zoomFolderPath = "";
	sl_int32 tilesNum = 1;

	sl_int32 packageX = (sl_int32)(location.x / m_nTilesXNum);
	sl_int32 packageY = (sl_int32)(location.y / m_nTilesYNum);
	outX = (sl_int32)location.x % m_nTilesXNum;
	outY = (sl_int32)location.y % m_nTilesYNum;

	String filePath = String::fromInt32(packageX) + _SLT(".pkg");
	String pkgPath = String::fromInt32(location.level) + _SLT("/") + String::fromInt32(packageY);
	
	return pkgPath + _SLT("/") + filePath;
}

SLIB_MAP_NAMESPACE_END
