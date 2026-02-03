/**
 *    Copyright (C) 2024-present BOONDB Contributors
 *
 *    This program is free software: you can redistribute it and/or modify
 *    it under the terms of the Server Side Public License, version 1,
 *    as published by MongoDB, Inc.
 */

#include "mongo/boon/uniform_array.h"

#include <cstring>

namespace mongo {
namespace boon {

// ========== Schema Implementation ==========

std::vector<char> Schema::serialize() const {
    std::vector<char> result;

    // Field count (2 bytes)
    uint16_t count = static_cast<uint16_t>(_fields.size());
    result.push_back(count & 0xFF);
    result.push_back((count >> 8) & 0xFF);

    // Each field
    for (const auto& field : _fields) {
        // Type (1 byte)
        result.push_back(static_cast<char>(field.type));

        // Name length (2 bytes)
        uint16_t nameLen = static_cast<uint16_t>(field.name.size());
        result.push_back(nameLen & 0xFF);
        result.push_back((nameLen >> 8) & 0xFF);

        // Name (no null terminator)
        result.insert(result.end(), field.name.begin(), field.name.end());
    }

    return result;
}

Schema Schema::deserialize(const char* data, size_t* bytesRead) {
    Schema schema;
    const char* ptr = data;

    // Field count
    uint16_t count = static_cast<uint8_t>(ptr[0]) |
                     (static_cast<uint8_t>(ptr[1]) << 8);
    ptr += 2;

    // Read fields
    for (uint16_t i = 0; i < count; ++i) {
        SchemaField field;

        // Type
        field.type = static_cast<BOONType>(*ptr++);

        // Name length
        uint16_t nameLen = static_cast<uint8_t>(ptr[0]) |
                          (static_cast<uint8_t>(ptr[1]) << 8);
        ptr += 2;

        // Name
        field.name = std::string(ptr, nameLen);
        ptr += nameLen;

        schema._fields.push_back(std::move(field));
    }

    if (bytesRead) {
        *bytesRead = ptr - data;
    }

    return schema;
}

// ========== UniformArray Implementation ==========

UniformArray::UniformArray(const char* data) : _data(data) {
    const char* ptr = data;

    // Schema size (4 bytes)
    uint32_t schemaSize = ConstDataView(ptr).read<LittleEndian<uint32_t>>();
    ptr += 4;

    // Parse schema
    size_t schemaBytesRead;
    _schema = Schema::deserialize(ptr, &schemaBytesRead);
    ptr += schemaSize;

    // Row count (4 bytes)
    _rowCount = ConstDataView(ptr).read<LittleEndian<uint32_t>>();
    ptr += 4;

    _rowDataStart = ptr - data;

    // Check if all fields are fixed size
    _fixedRowSize = _schema.allFixedSize();
    if (_fixedRowSize) {
        _rowSize = _schema.fixedRowSize();
        _totalSize = _rowDataStart + (_rowCount * _rowSize);
    } else {
        // For variable-size rows, we need to scan through
        computeRowOffsets();
        if (_rowCount > 0) {
            _totalSize = _rowOffsets[_rowCount];
        } else {
            _totalSize = _rowDataStart;
        }
    }
}

void UniformArray::computeRowOffsets() const {
    if (_rowOffsetsComputed) return;

    _rowOffsets.resize(_rowCount + 1);
    _rowOffsets[0] = _rowDataStart;

    const char* ptr = _data + _rowDataStart;

    for (size_t row = 0; row < _rowCount; ++row) {
        for (size_t field = 0; field < _schema.numFields(); ++field) {
            const auto& f = _schema.field(field);

            if (isFixedSizeType(f.type)) {
                ptr += fixedSize(f.type);
            } else {
                // Variable size: read length prefix
                uint32_t len = ConstDataView(ptr).read<LittleEndian<uint32_t>>();
                ptr += 4 + len;
            }
        }
        _rowOffsets[row + 1] = ptr - _data;
    }

    _rowOffsetsComputed = true;
}

size_t UniformArray::rowOffset(size_t rowIndex) const {
    if (_fixedRowSize) {
        return _rowDataStart + (rowIndex * _rowSize);
    }
    computeRowOffsets();
    return _rowOffsets[rowIndex];
}

size_t UniformArray::fieldOffset(size_t rowIndex, size_t fieldIndex) const {
    size_t offset = rowOffset(rowIndex);

    for (size_t i = 0; i < fieldIndex; ++i) {
        const auto& f = _schema.field(i);
        if (isFixedSizeType(f.type)) {
            offset += fixedSize(f.type);
        } else {
            uint32_t len = ConstDataView(_data + offset).read<LittleEndian<uint32_t>>();
            offset += 4 + len;
        }
    }

    return offset;
}

// ========== Row Implementation ==========

const char* UniformArray::Row::getValuePtr(size_t fieldIndex) const {
    size_t offset = _parent->fieldOffset(_rowIndex, fieldIndex);
    return _parent->_data + offset;
}

double UniformArray::Row::getDouble(size_t fieldIndex) const {
    return ConstDataView(getValuePtr(fieldIndex)).read<LittleEndian<double>>();
}

int32_t UniformArray::Row::getInt32(size_t fieldIndex) const {
    return ConstDataView(getValuePtr(fieldIndex)).read<LittleEndian<int32_t>>();
}

int64_t UniformArray::Row::getInt64(size_t fieldIndex) const {
    return ConstDataView(getValuePtr(fieldIndex)).read<LittleEndian<int64_t>>();
}

StringData UniformArray::Row::getString(size_t fieldIndex) const {
    const char* ptr = getValuePtr(fieldIndex);
    uint32_t len = ConstDataView(ptr).read<LittleEndian<uint32_t>>();
    return StringData(ptr + 4, len);
}

bool UniformArray::Row::getBool(size_t fieldIndex) const {
    return *getValuePtr(fieldIndex) != 0;
}

// ========== UniformArrayBuilder Implementation ==========

void UniformArrayBuilder::appendDouble(double val) {
    size_t oldSize = _currentRow.size();
    _currentRow.resize(oldSize + 8);
    DataView(_currentRow.data() + oldSize).write<LittleEndian<double>>(val);
}

void UniformArrayBuilder::appendInt32(int32_t val) {
    size_t oldSize = _currentRow.size();
    _currentRow.resize(oldSize + 4);
    DataView(_currentRow.data() + oldSize).write<LittleEndian<int32_t>>(val);
}

void UniformArrayBuilder::appendInt64(int64_t val) {
    size_t oldSize = _currentRow.size();
    _currentRow.resize(oldSize + 8);
    DataView(_currentRow.data() + oldSize).write<LittleEndian<int64_t>>(val);
}

void UniformArrayBuilder::appendString(StringData val) {
    size_t oldSize = _currentRow.size();
    _currentRow.resize(oldSize + 4 + val.size());

    // Length prefix
    DataView(_currentRow.data() + oldSize).write<LittleEndian<uint32_t>>(val.size());

    // String data (no null terminator)
    memcpy(_currentRow.data() + oldSize + 4, val.rawData(), val.size());
}

void UniformArrayBuilder::appendBool(bool val) {
    _currentRow.push_back(val ? 1 : 0);
}

std::vector<char> UniformArrayBuilder::done() {
    std::vector<char> result;

    // Serialize schema
    auto schemaBytes = _schema.serialize();

    // Schema size (4 bytes)
    uint32_t schemaSize = static_cast<uint32_t>(schemaBytes.size());
    result.resize(4);
    DataView(result.data()).write<LittleEndian<uint32_t>>(schemaSize);

    // Schema data
    result.insert(result.end(), schemaBytes.begin(), schemaBytes.end());

    // Row count (4 bytes)
    size_t rowCountOffset = result.size();
    result.resize(result.size() + 4);
    DataView(result.data() + rowCountOffset).write<LittleEndian<uint32_t>>(_rows.size());

    // Row data
    for (const auto& row : _rows) {
        result.insert(result.end(), row.begin(), row.end());
    }

    return result;
}

}  // namespace boon
}  // namespace mongo
