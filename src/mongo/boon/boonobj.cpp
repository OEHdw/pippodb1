/**
 *    Copyright (C) 2024-present BOONDB Contributors
 *
 *    This program is free software: you can redistribute it and/or modify
 *    it under the terms of the Server Side Public License, version 1,
 *    as published by MongoDB, Inc.
 */

#include "mongo/boon/boonobj.h"
#include "mongo/boon/uniform_array.h"
#include "mongo/bson/bsonobj.h"
#include "mongo/bson/bsonobjbuilder.h"

#include <cstring>
#include <sstream>

namespace mongo {

// ========== BOONObj Implementation ==========

BOONObj::BOONObj() : _data(kEmptyObject), _buffer(nullptr) {}

BOONObj::BOONObj(const char* data) : _data(data), _buffer(nullptr) {}

BOONObj::BOONObj(std::shared_ptr<const char[]> buffer, const char* data)
    : _data(data), _buffer(std::move(buffer)) {}

int BOONObj::nFields() const {
    int count = 0;
    for (auto it = begin(); it.more(); it.next()) {
        ++count;
    }
    return count;
}

bool BOONObj::isValid() const {
    if (objsize() < 5) return false;
    if (_data[objsize() - 1] != 0) return false;  // Must end with EOO
    return true;
}

BOONElement BOONObj::getField(StringData field) const {
    for (auto it = begin(); it.more(); ) {
        BOONElement elem = it.next();
        if (elem.fieldNameStringData() == field) {
            return elem;
        }
    }
    return BOONElement();  // EOO
}

BOONObj BOONObj::getOwned() const {
    if (isOwned()) {
        return *this;
    }

    int size = objsize();
    auto buffer = std::shared_ptr<char[]>(new char[size]);
    memcpy(buffer.get(), _data, size);

    return BOONObj(std::reinterpret_pointer_cast<const char[]>(buffer),
                   buffer.get());
}

std::string BOONObj::toString() const {
    std::ostringstream ss;
    ss << "{ ";

    bool first = true;
    for (auto it = begin(); it.more(); ) {
        BOONElement elem = it.next();
        if (!first) ss << ", ";
        first = false;

        ss << "\"" << elem.fieldNameStringData() << "\": ";

        switch (elem.boonType()) {
            case boon::BOONType::NumberDouble:
                ss << elem.Double();
                break;
            case boon::BOONType::String:
                ss << "\"" << elem.String() << "\"";
                break;
            case boon::BOONType::Object:
                ss << elem.Obj().toString();
                break;
            case boon::BOONType::Array:
                ss << elem.Array().toString();
                break;
            case boon::BOONType::Bool:
                ss << (elem.Bool() ? "true" : "false");
                break;
            case boon::BOONType::jstNULL:
                ss << "null";
                break;
            case boon::BOONType::NumberInt:
                ss << elem.Int();
                break;
            case boon::BOONType::NumberLong:
                ss << elem.Long();
                break;
            case boon::BOONType::UniformArray:
                ss << "[UniformArray: " << elem.uniformArray().numRows() << " rows]";
                break;
            case boon::BOONType::CompactArray:
                ss << "[CompactArray]";
                break;
            default:
                ss << "<" << boon::typeName(elem.boonType()) << ">";
                break;
        }
    }

    ss << " }";
    return ss.str();
}

BOONObj::Stats BOONObj::getStats() const {
    Stats stats = {0, 0, 0, 0, 0};
    stats.totalSize = objsize();

    for (auto it = begin(); it.more(); ) {
        BOONElement elem = it.next();
        stats.numFields++;

        if (elem.isUniformArray()) {
            stats.numUniformArrays++;
            // Estimate saved bytes: assume 40% savings on uniform arrays
            // This is a rough estimate
            stats.savedBytes += elem.valueSize() * 2 / 5;
        } else if (elem.isCompactArray()) {
            stats.numCompactArrays++;
            // Compact arrays save index keys ("0", "1", "2", ...)
            // Rough estimate: 3 bytes per element average
        }
    }

    return stats;
}

int BOONObj::woCompare(const BOONObj& other) const {
    auto it1 = begin();
    auto it2 = other.begin();

    while (it1.more() && it2.more()) {
        BOONElement e1 = it1.next();
        BOONElement e2 = it2.next();

        int cmp = e1.woCompare(e2, true);
        if (cmp != 0) return cmp;
    }

    if (it1.more()) return 1;
    if (it2.more()) return -1;
    return 0;
}

// ========== BOONObjBuilder Implementation ==========

BOONObjBuilder::BOONObjBuilder() {
    // Reserve space for size (will be filled in at end)
    _buffer.resize(4);
}

void BOONObjBuilder::appendType(boon::BOONType type) {
    _buffer.push_back(static_cast<char>(type));
}

void BOONObjBuilder::appendFieldName(StringData field) {
    _buffer.insert(_buffer.end(), field.rawData(), field.rawData() + field.size());
    _buffer.push_back('\0');
}

void BOONObjBuilder::appendBytes(const char* data, size_t len) {
    _buffer.insert(_buffer.end(), data, data + len);
}

template <typename T>
void BOONObjBuilder::appendNum(T val) {
    size_t oldSize = _buffer.size();
    _buffer.resize(oldSize + sizeof(T));
    DataView(_buffer.data() + oldSize).write<LittleEndian<T>>(val);
}

BOONObjBuilder& BOONObjBuilder::append(StringData field, double val) {
    appendType(boon::BOONType::NumberDouble);
    appendFieldName(field);
    appendNum(val);
    _numFields++;
    return *this;
}

BOONObjBuilder& BOONObjBuilder::append(StringData field, StringData val) {
    appendType(boon::BOONType::String);
    appendFieldName(field);
    // String format: <len:4><data><null>
    appendNum<int32_t>(val.size() + 1);  // +1 for null terminator
    appendBytes(val.rawData(), val.size());
    _buffer.push_back('\0');
    _numFields++;
    return *this;
}

BOONObjBuilder& BOONObjBuilder::append(StringData field, int32_t val) {
    appendType(boon::BOONType::NumberInt);
    appendFieldName(field);
    appendNum(val);
    _numFields++;
    return *this;
}

BOONObjBuilder& BOONObjBuilder::append(StringData field, int64_t val) {
    appendType(boon::BOONType::NumberLong);
    appendFieldName(field);
    appendNum(val);
    _numFields++;
    return *this;
}

BOONObjBuilder& BOONObjBuilder::append(StringData field, bool val) {
    appendType(boon::BOONType::Bool);
    appendFieldName(field);
    _buffer.push_back(val ? 1 : 0);
    _numFields++;
    return *this;
}

BOONObjBuilder& BOONObjBuilder::appendNull(StringData field) {
    appendType(boon::BOONType::jstNULL);
    appendFieldName(field);
    _numFields++;
    return *this;
}

BOONObjBuilder& BOONObjBuilder::append(StringData field, const OID& oid) {
    appendType(boon::BOONType::OID);
    appendFieldName(field);
    appendBytes(oid.view().view(), OID::kOIDSize);
    _numFields++;
    return *this;
}

BOONObjBuilder& BOONObjBuilder::append(StringData field, const BOONObj& obj) {
    appendType(boon::BOONType::Object);
    appendFieldName(field);
    appendBytes(obj.objdata(), obj.objsize());
    _numFields++;
    return *this;
}

BOONObjBuilder& BOONObjBuilder::appendUniformArray(StringData field,
                                                    const boon::UniformArray& arr) {
    appendType(boon::BOONType::UniformArray);
    appendFieldName(field);
    appendBytes(arr.rawdata(), arr.size());
    _numFields++;
    return *this;
}

BOONObj BOONObjBuilder::obj() {
    // Append EOO
    _buffer.push_back(0);

    // Write total size at beginning
    DataView(_buffer.data()).write<LittleEndian<int32_t>>(_buffer.size());

    // Create owned BOONObj
    auto buffer = std::shared_ptr<char[]>(new char[_buffer.size()]);
    memcpy(buffer.get(), _buffer.data(), _buffer.size());

    return BOONObj(std::reinterpret_pointer_cast<const char[]>(buffer),
                   buffer.get());
}

// ========== BOONElement Implementation ==========

int BOONElement::size() const {
    if (eoo()) return 1;

    int valueOffset = 1 + fieldNameSize() + 1;  // type + name + null
    int valSize = valueSize();

    return valueOffset + valSize;
}

int BOONElement::valueSize() const {
    auto type = boonType();

    // Fixed-size types
    if (boon::isFixedSizeType(type)) {
        return boon::fixedSize(type);
    }

    const char* v = value();

    switch (type) {
        case boon::BOONType::String:
        case boon::BOONType::Code:
        case boon::BOONType::Symbol: {
            // <len:4><data>
            int32_t len = ConstDataView(v).read<LittleEndian<int32_t>>();
            return 4 + len;
        }

        case boon::BOONType::Object:
        case boon::BOONType::Array: {
            // Embedded object has size at start
            return ConstDataView(v).read<LittleEndian<int32_t>>();
        }

        case boon::BOONType::BinData: {
            // <len:4><subtype:1><data>
            int32_t len = ConstDataView(v).read<LittleEndian<int32_t>>();
            return 4 + 1 + len;
        }

        case boon::BOONType::RegEx: {
            // <pattern:cstring><options:cstring>
            const char* p = v;
            p += strlen(p) + 1;  // pattern
            p += strlen(p) + 1;  // options
            return p - v;
        }

        case boon::BOONType::CodeWScope: {
            // <totalSize:4>...
            return ConstDataView(v).read<LittleEndian<int32_t>>();
        }

        case boon::BOONType::UniformArray:
        case boon::BOONType::CompactArray: {
            // BOON extension: need to parse to get size
            // For now, use schemaSize + data
            // Format: <schemaSize:4><schema><rowCount:4><rows>
            // TODO: Implement proper size calculation
            uint32_t schemaSize = ConstDataView(v).read<LittleEndian<uint32_t>>();
            // This is a simplified version - proper impl needs to scan rows
            return schemaSize + 100;  // Placeholder
        }

        default:
            return 0;
    }
}

int BOONElement::woCompare(const BOONElement& other, bool considerFieldName) const {
    if (considerFieldName) {
        int cmp = fieldNameStringData().compare(other.fieldNameStringData());
        if (cmp != 0) return cmp;
    }

    // Compare by canonical type first
    int8_t lType = boon::canonicalType(boonType());
    int8_t rType = boon::canonicalType(other.boonType());

    if (lType != rType) {
        return lType < rType ? -1 : 1;
    }

    // Same canonical type - compare values
    auto type = boonType();

    switch (type) {
        case boon::BOONType::NumberDouble:
        case boon::BOONType::NumberInt:
        case boon::BOONType::NumberLong:
        case boon::BOONType::NumberDecimal: {
            double l = number();
            double r = other.number();
            if (l < r) return -1;
            if (l > r) return 1;
            return 0;
        }

        case boon::BOONType::String:
        case boon::BOONType::Symbol:
            return valueStringData().compare(other.valueStringData());

        case boon::BOONType::Bool:
            return Bool() - other.Bool();

        case boon::BOONType::Object:
            return Obj().woCompare(other.Obj());

        case boon::BOONType::Array:
        case boon::BOONType::UniformArray:
        case boon::BOONType::CompactArray:
            // TODO: Proper array comparison
            return 0;

        default:
            // Binary comparison for other types
            return memcmp(value(), other.value(),
                         std::min(valueSize(), other.valueSize()));
    }
}

boon::UniformArray BOONElement::uniformArray() const {
    return boon::UniformArray(value());
}

}  // namespace mongo
