//! # BOONDB
//!
//! A document database with native BOON (Binary Object-Oriented Notation) support.
//!
//! BOON is a compact binary format optimized for:
//! - Uniform arrays (tabular data stored efficiently)
//! - LLM token optimization
//! - Fast serialization/deserialization
//!
//! ## Architecture
//!
//! ```text
//! ┌─────────────────────────────────────────────────────────────────┐
//! │                         BOONDB                                   │
//! ├─────────────────────────────────────────────────────────────────┤
//! │  Layer 5: API          REST API / CLI                           │
//! │  Layer 4: Query        Parser → Executor                        │
//! │  Layer 3: Collections  Namespace → Metadata → Documents         │
//! │  Layer 2: Storage      RecordStore → SQLite/Files               │
//! │  Layer 1: BOON Core    Document → Element → Value               │
//! └─────────────────────────────────────────────────────────────────┘
//! ```
//!
//! ## Quick Start
//!
//! ```rust,ignore
//! use boondb::boon::{Document, Value};
//!
//! // Create a document
//! let mut doc = Document::new();
//! doc.insert("name", "Mario");
//! doc.insert("age", 30);
//! doc.insert("active", true);
//!
//! // Serialize to BOON binary
//! let bytes = doc.to_bytes();
//!
//! // Deserialize
//! let doc2 = Document::from_bytes(&bytes)?;
//! ```

#![warn(missing_docs)]
#![warn(rust_2018_idioms)]

pub mod boon;
pub mod storage;
pub mod query;
pub mod error;

// Re-exports for convenience
pub use boon::{Document, Element, Value, BoonType};
pub use error::{Error, Result};
