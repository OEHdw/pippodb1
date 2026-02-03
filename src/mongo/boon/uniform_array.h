/**
 *    Copyright (C) 2024-present BOONDB Contributors
 *
 *    This program is free software: you can redistribute it and/or modify
 *    it under the terms of the Server Side Public License, version 1,
 *    as published by MongoDB, Inc.
 */

#pragma once

#include "mongo/boon/boontypes.h"
#include "mongo/base/data_view.h"
#include "mongo/base/string_data.h"

#include <cstdint>
#include <string>
#include <vector>

namespace mongo {
namespace boon {

/**
 * SchemaField describes a single field in a UniformArray schema.
 */
struct SchemaField {
    std::string name;
    BOONType type;

    SchemaField() = default;
    SchemaField(std::string n, BOONType t) : name(std::move(n)), type(t) {}

    bool operator==(const SchemaField& other) const {
        return name == other.name && type == other.type;
    }
};

/**
 * Schema describes the structure of rows in a UniformArray.
 *
 * Example:
 *   Schema: [{name: "id", type: Int32}, {name: "value", type: String}]
 *
 * This schema would describe rows like:
 *   [1, "hello"], [2, "world"], [3, "!"]
 */
class Schema {
public:
    Schema() = default;

    /**
     * Add a field to the schema.
     */
    void addField(const std::string& name, BOONType type) {
        _fields.emplace_back(name, type);
    }

    /**
     * Get number of fields.
     */
    size_t numFields() const {
        return _fields.size();
    }

    /**
     * Get field at index.
     */
    const SchemaField& field(size_t index) const {
        return _fields[index];
    }

    /**
     * Get all fields.
     */
    const std::vector<SchemaField>& fields() const {
        return _fields;
    }

    /**
     * Find field index by name. Returns -1 if not found.
     */
    int fieldIndex(StringData name) const {
        for (size_t i = 0; i < _fields.size(); ++i) {
            if (_fields[i].name == name) {
                return static_cast<int>(i);
            }
        }
        return -1;
    }

    /**
     * Check if all fields have fixed size.
     * If true, we can use optimized row access.
     */
    bool allFixedSize() const {
        for (const auto& f : _fields) {
            if (!isFixedSizeType(f.type)) {
                return false;
            }
        }
        return true;
    }

    /**
     * Calculate fixed row size (only valid if allFixedSize() is true).
     */
    size_t fixedRowSize() const {
        size_t size = 0;
        for (const auto& f : _fields) {
            size += boon::fixedSize(f.type);
        }
        return size;
    }

    /**
     * Serialize schema to binary format.
     *
     * Format:
     *   <fieldCount:2><field1>...<fieldN>
     * Where each field is:
     *   <type:1><nameLen:2><name:nameLen>
     */
    std::vector<char> serialize() const;

    /**
     * Deserialize schema from binary data.
     * Returns number of bytes consumed.
     */
    static Schema deserialize(const char* data, size_t* bytesRead = nullptr);

    bool operator==(const Schema& other) const {
        return _fields == other._fields;
    }

private:
    std::vector<SchemaField> _fields;
};

/**
 * UniformArray - THE KEY INNOVATION OF BOON
 *
 * A UniformArray stores an array of objects that all have the same schema.
 * Instead of repeating field names for every element, we store:
 * 1. The schema (field names and types) ONCE
 * 2. Row data as packed values (no field names!)
 *
 * Example - Traditional BSON array:
 *   [{name:"A", age:1}, {name:"B", age:2}, {name:"C", age:3}]
 *
 *   Stored as:
 *   "0" -> {name:"A", age:1}  // "name" and "age" repeated
 *   "1" -> {name:"B", age:2}  // "name" and "age" repeated
 *   "2" -> {name:"C", age:3}  // "name" and "age" repeated
 *
 *   Total: ~120 bytes
 *
 * UniformArray format:
 *   Schema: [(name, String), (age, Int32)]  // stored ONCE
 *   Rows: [["A", 1], ["B", 2], ["C", 3]]    // just values, no keys
 *
 *   Total: ~50 bytes  (~60% smaller!)
 *
 * Binary layout:
 *   <schemaSize:4><schema><rowCount:4><row1><row2>...
 *
 * Where each row is:
 *   For fixed-size types: just the value bytes
 *   For variable-size types: <size:4><value bytes>
 */
class UniformArray {
public:
    /**
     * Row provides access to a single row in the UniformArray.
     */
    class Row {
    public:
        Row(const UniformArray* parent, size_t rowIndex)
            : _parent(parent), _rowIndex(rowIndex) {}

        /**
         * Get value at field index as double.
         */
        double getDouble(size_t fieldIndex) const;

        /**
         * Get value at field index as int32.
         */
        int32_t getInt32(size_t fieldIndex) const;

        /**
         * Get value at field index as int64.
         */
        int64_t getInt64(size_t fieldIndex) const;

        /**
         * Get value at field index as string.
         */
        StringData getString(size_t fieldIndex) const;

        /**
         * Get value at field index as bool.
         */
        bool getBool(size_t fieldIndex) const;

        /**
         * Get raw pointer to value at field index.
         */
        const char* getValuePtr(size_t fieldIndex) const;

    private:
        const UniformArray* _parent;
        size_t _rowIndex;
    };

    /**
     * Iterator for rows.
     */
    class Iterator {
    public:
        Iterator(const UniformArray* parent, size_t index)
            : _parent(parent), _index(index) {}

        Row operator*() const {
            return Row(_parent, _index);
        }

        Iterator& operator++() {
            ++_index;
            return *this;
        }

        bool operator!=(const Iterator& other) const {
            return _index != other._index;
        }

    private:
        const UniformArray* _parent;
        size_t _index;
    };

    UniformArray() = default;

    /**
     * Construct from raw binary data.
     */
    explicit UniformArray(const char* data);

    /**
     * Get the schema.
     */
    const Schema& schema() const {
        return _schema;
    }

    /**
     * Get number of rows.
     */
    size_t numRows() const {
        return _rowCount;
    }

    /**
     * Get row at index.
     */
    Row row(size_t index) const {
        return Row(this, index);
    }

    /**
     * Begin iterator.
     */
    Iterator begin() const {
        return Iterator(this, 0);
    }

    /**
     * End iterator.
     */
    Iterator end() const {
        return Iterator(this, _rowCount);
    }

    /**
     * Get total size in bytes.
     */
    size_t size() const {
        return _totalSize;
    }

    /**
     * Get raw data pointer.
     */
    const char* rawdata() const {
        return _data;
    }

    /**
     * Calculate byte offset to start of row at given index.
     */
    size_t rowOffset(size_t rowIndex) const;

    /**
     * Calculate byte offset to field within a row.
     */
    size_t fieldOffset(size_t rowIndex, size_t fieldIndex) const;

private:
    const char* _data = nullptr;
    Schema _schema;
    size_t _rowCount = 0;
    size_t _totalSize = 0;
    size_t _rowDataStart = 0;  // Offset to first row

    // For fixed-size schemas, we can precompute row size
    bool _fixedRowSize = false;
    size_t _rowSize = 0;

    // Row offsets for variable-size rows (computed lazily)
    mutable std::vector<size_t> _rowOffsets;
    mutable bool _rowOffsetsComputed = false;

    void computeRowOffsets() const;
};

/**
 * UniformArrayBuilder - Build a UniformArray incrementally.
 */
class UniformArrayBuilder {
public:
    UniformArrayBuilder() = default;

    /**
     * Set the schema. Must be called before adding rows.
     */
    void setSchema(Schema schema) {
        _schema = std::move(schema);
    }

    /**
     * Start a new row.
     */
    void startRow() {
        _currentRow.clear();
    }

    /**
     * Append a double value to current row.
     */
    void appendDouble(double val);

    /**
     * Append an int32 value to current row.
     */
    void appendInt32(int32_t val);

    /**
     * Append an int64 value to current row.
     */
    void appendInt64(int64_t val);

    /**
     * Append a string value to current row.
     */
    void appendString(StringData val);

    /**
     * Append a bool value to current row.
     */
    void appendBool(bool val);

    /**
     * Finish current row and add to array.
     */
    void finishRow() {
        _rows.push_back(std::move(_currentRow));
        _currentRow.clear();
    }

    /**
     * Get number of rows added so far.
     */
    size_t numRows() const {
        return _rows.size();
    }

    /**
     * Build the final binary representation.
     */
    std::vector<char> done();

private:
    Schema _schema;
    std::vector<char> _currentRow;
    std::vector<std::vector<char>> _rows;
};

}  // namespace boon
}  // namespace mongo
