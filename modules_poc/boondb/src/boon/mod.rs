//! # BOON - Binary Object-Oriented Notation
//!
//! BOON is a compact binary format for document storage, inspired by BSON
//! but optimized for uniform arrays (tabular data).
//!
//! ## Key Innovation: Uniform Arrays
//!
//! Traditional BSON stores arrays of objects redundantly:
//! ```json
//! [
//!   {"name": "Mario", "age": 30},
//!   {"name": "Luigi", "age": 25},
//!   {"name": "Peach", "age": 28}
//! ]
//! ```
//! Keys "name" and "age" are repeated 3 times!
//!
//! BOON's UniformArray stores the schema once:
//! ```text
//! UniformArray {
//!   schema: ["name": String, "age": Int32]
//!   rows: [
//!     ["Mario", 30],
//!     ["Luigi", 25],
//!     ["Peach", 28]
//!   ]
//! }
//! ```
//! ~40% smaller for typical data!

mod types;
mod value;
mod element;
mod document;
mod uniform_array;
mod serializer;

pub use types::BoonType;
pub use value::Value;
pub use element::Element;
pub use document::Document;
pub use uniform_array::{UniformArray, Schema, SchemaField};
pub use serializer::{Serializer, Deserializer};
