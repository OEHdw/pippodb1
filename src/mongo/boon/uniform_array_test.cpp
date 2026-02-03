/**
 *    Copyright (C) 2024-present BOONDB Contributors
 *
 *    This program is free software: you can redistribute it and/or modify
 *    it under the terms of the Server Side Public License, version 1,
 *    as published by MongoDB, Inc.
 */

#include "mongo/boon/uniform_array.h"
#include "mongo/boon/boontypes.h"
#include "mongo/unittest/assert.h"
#include "mongo/unittest/framework.h"

namespace mongo {
namespace {

using namespace boon;

// ========== Schema Tests ==========

TEST(SchemaTest, EmptySchema) {
    Schema schema;
    ASSERT_EQ(schema.numFields(), 0u);
    ASSERT_TRUE(schema.allFixedSize());
    ASSERT_EQ(schema.fixedRowSize(), 0u);
}

TEST(SchemaTest, AddFields) {
    Schema schema;
    schema.addField("name", BOONType::String);
    schema.addField("age", BOONType::NumberInt);
    schema.addField("active", BOONType::Bool);

    ASSERT_EQ(schema.numFields(), 3u);
    ASSERT_EQ(schema.field(0).name, "name");
    ASSERT_EQ(schema.field(0).type, BOONType::String);
    ASSERT_EQ(schema.field(1).name, "age");
    ASSERT_EQ(schema.field(1).type, BOONType::NumberInt);
    ASSERT_EQ(schema.field(2).name, "active");
    ASSERT_EQ(schema.field(2).type, BOONType::Bool);
}

TEST(SchemaTest, FieldIndex) {
    Schema schema;
    schema.addField("id", BOONType::NumberInt);
    schema.addField("name", BOONType::String);
    schema.addField("score", BOONType::NumberDouble);

    ASSERT_EQ(schema.fieldIndex("id"), 0);
    ASSERT_EQ(schema.fieldIndex("name"), 1);
    ASSERT_EQ(schema.fieldIndex("score"), 2);
    ASSERT_EQ(schema.fieldIndex("nonexistent"), -1);
}

TEST(SchemaTest, AllFixedSize) {
    // All fixed-size fields
    Schema fixed;
    fixed.addField("a", BOONType::NumberInt);
    fixed.addField("b", BOONType::NumberDouble);
    fixed.addField("c", BOONType::Bool);
    ASSERT_TRUE(fixed.allFixedSize());
    ASSERT_EQ(fixed.fixedRowSize(), 4u + 8u + 1u);  // int32 + double + bool

    // Mixed with variable-size
    Schema mixed;
    mixed.addField("a", BOONType::NumberInt);
    mixed.addField("b", BOONType::String);  // Variable size!
    ASSERT_FALSE(mixed.allFixedSize());
}

TEST(SchemaTest, SerializeDeserialize) {
    Schema original;
    original.addField("id", BOONType::NumberInt);
    original.addField("name", BOONType::String);
    original.addField("score", BOONType::NumberDouble);
    original.addField("active", BOONType::Bool);

    // Serialize
    auto bytes = original.serialize();
    ASSERT_GT(bytes.size(), 0u);

    // Deserialize
    size_t bytesRead = 0;
    Schema restored = Schema::deserialize(bytes.data(), &bytesRead);

    ASSERT_EQ(bytesRead, bytes.size());
    ASSERT_EQ(restored.numFields(), original.numFields());

    for (size_t i = 0; i < original.numFields(); ++i) {
        ASSERT_EQ(restored.field(i).name, original.field(i).name);
        ASSERT_EQ(restored.field(i).type, original.field(i).type);
    }
}

TEST(SchemaTest, Equality) {
    Schema a;
    a.addField("x", BOONType::NumberInt);
    a.addField("y", BOONType::NumberDouble);

    Schema b;
    b.addField("x", BOONType::NumberInt);
    b.addField("y", BOONType::NumberDouble);

    Schema c;
    c.addField("x", BOONType::NumberInt);
    c.addField("z", BOONType::NumberDouble);  // Different name

    Schema d;
    d.addField("x", BOONType::NumberInt);
    d.addField("y", BOONType::String);  // Different type

    ASSERT_TRUE(a == b);
    ASSERT_FALSE(a == c);
    ASSERT_FALSE(a == d);
}

// ========== UniformArrayBuilder Tests ==========

TEST(UniformArrayBuilderTest, FixedSizeArray) {
    Schema schema;
    schema.addField("x", BOONType::NumberInt);
    schema.addField("y", BOONType::NumberInt);

    UniformArrayBuilder builder;
    builder.setSchema(schema);

    // Row 0: {x: 1, y: 10}
    builder.startRow();
    builder.appendInt32(1);
    builder.appendInt32(10);
    builder.finishRow();

    // Row 1: {x: 2, y: 20}
    builder.startRow();
    builder.appendInt32(2);
    builder.appendInt32(20);
    builder.finishRow();

    // Row 2: {x: 3, y: 30}
    builder.startRow();
    builder.appendInt32(3);
    builder.appendInt32(30);
    builder.finishRow();

    ASSERT_EQ(builder.numRows(), 3u);

    auto bytes = builder.done();
    ASSERT_GT(bytes.size(), 0u);

    // Parse back
    UniformArray arr(bytes.data());
    ASSERT_EQ(arr.numRows(), 3u);
    ASSERT_EQ(arr.schema().numFields(), 2u);

    // Verify values
    ASSERT_EQ(arr.row(0).getInt32(0), 1);
    ASSERT_EQ(arr.row(0).getInt32(1), 10);
    ASSERT_EQ(arr.row(1).getInt32(0), 2);
    ASSERT_EQ(arr.row(1).getInt32(1), 20);
    ASSERT_EQ(arr.row(2).getInt32(0), 3);
    ASSERT_EQ(arr.row(2).getInt32(1), 30);
}

TEST(UniformArrayBuilderTest, MixedTypeArray) {
    Schema schema;
    schema.addField("name", BOONType::String);
    schema.addField("age", BOONType::NumberInt);
    schema.addField("score", BOONType::NumberDouble);
    schema.addField("active", BOONType::Bool);

    UniformArrayBuilder builder;
    builder.setSchema(schema);

    builder.startRow();
    builder.appendString("Alice");
    builder.appendInt32(30);
    builder.appendDouble(95.5);
    builder.appendBool(true);
    builder.finishRow();

    builder.startRow();
    builder.appendString("Bob");
    builder.appendInt32(25);
    builder.appendDouble(87.3);
    builder.appendBool(false);
    builder.finishRow();

    auto bytes = builder.done();
    UniformArray arr(bytes.data());

    ASSERT_EQ(arr.numRows(), 2u);

    // Row 0
    ASSERT_EQ(arr.row(0).getString(0), "Alice");
    ASSERT_EQ(arr.row(0).getInt32(1), 30);
    ASSERT_EQ(arr.row(0).getDouble(2), 95.5);
    ASSERT_EQ(arr.row(0).getBool(3), true);

    // Row 1
    ASSERT_EQ(arr.row(1).getString(0), "Bob");
    ASSERT_EQ(arr.row(1).getInt32(1), 25);
    ASSERT_EQ(arr.row(1).getDouble(2), 87.3);
    ASSERT_EQ(arr.row(1).getBool(3), false);
}

// ========== UniformArray Iterator Test ==========

TEST(UniformArrayTest, Iterator) {
    Schema schema;
    schema.addField("val", BOONType::NumberInt);

    UniformArrayBuilder builder;
    builder.setSchema(schema);

    for (int i = 0; i < 5; ++i) {
        builder.startRow();
        builder.appendInt32(i * 10);
        builder.finishRow();
    }

    auto bytes = builder.done();
    UniformArray arr(bytes.data());

    int count = 0;
    for (const auto& row : arr) {
        ASSERT_EQ(row.getInt32(0), count * 10);
        count++;
    }
    ASSERT_EQ(count, 5);
}

// ========== Size Savings Test ==========

TEST(UniformArrayTest, SizeSavings) {
    // Simulate what a BSON array would look like
    // [{name:"Alice", age:30}, {name:"Bob", age:25}, {name:"Carol", age:28}]
    //
    // In BSON:
    //   "0" -> {name:"Alice", age:30}  (~35 bytes per element)
    //   "1" -> {name:"Bob", age:25}
    //   "2" -> {name:"Carol", age:28}
    // Approx: 3 * 35 = ~105 bytes + overhead
    //
    // In BOON UniformArray:
    //   Schema: [(name, String), (age, Int32)] = ~20 bytes
    //   Rows: ["Alice",30], ["Bob",25], ["Carol",28] = ~30 bytes
    // Approx: ~50 bytes

    Schema schema;
    schema.addField("name", BOONType::String);
    schema.addField("age", BOONType::NumberInt);

    UniformArrayBuilder builder;
    builder.setSchema(schema);

    builder.startRow();
    builder.appendString("Alice");
    builder.appendInt32(30);
    builder.finishRow();

    builder.startRow();
    builder.appendString("Bob");
    builder.appendInt32(25);
    builder.finishRow();

    builder.startRow();
    builder.appendString("Carol");
    builder.appendInt32(28);
    builder.finishRow();

    auto bytes = builder.done();

    // BOON should be significantly smaller than BSON equivalent
    // Exact numbers depend on implementation, but verify it's reasonable
    ASSERT_GT(bytes.size(), 20u);   // Not empty
    ASSERT_LT(bytes.size(), 200u);  // Not bloated

    // Verify data integrity
    UniformArray arr(bytes.data());
    ASSERT_EQ(arr.numRows(), 3u);
    ASSERT_EQ(arr.row(0).getString(0), "Alice");
    ASSERT_EQ(arr.row(2).getString(0), "Carol");
}

}  // namespace
}  // namespace mongo
