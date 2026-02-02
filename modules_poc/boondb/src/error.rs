//! Error types for BOONDB

use thiserror::Error;

/// Main error type for BOONDB operations
#[derive(Error, Debug)]
pub enum Error {
    /// Error during BOON serialization
    #[error("Serialization error: {0}")]
    Serialization(String),

    /// Error during BOON deserialization
    #[error("Deserialization error: {0}")]
    Deserialization(String),

    /// Invalid BOON type encountered
    #[error("Invalid BOON type: {0}")]
    InvalidType(u8),

    /// Field not found in document
    #[error("Field not found: {0}")]
    FieldNotFound(String),

    /// Invalid document structure
    #[error("Invalid document: {0}")]
    InvalidDocument(String),

    /// Storage error
    #[error("Storage error: {0}")]
    Storage(String),

    /// Query error
    #[error("Query error: {0}")]
    Query(String),

    /// I/O error
    #[error("I/O error: {0}")]
    Io(#[from] std::io::Error),

    /// UTF-8 conversion error
    #[error("UTF-8 error: {0}")]
    Utf8(#[from] std::string::FromUtf8Error),

    /// Buffer too small
    #[error("Buffer too small: need {needed} bytes, have {available}")]
    BufferTooSmall { needed: usize, available: usize },

    /// Unexpected end of data
    #[error("Unexpected end of data at position {position}")]
    UnexpectedEof { position: usize },

    /// Schema mismatch in uniform array
    #[error("Schema mismatch in uniform array: expected {expected}, got {got}")]
    SchemaMismatch { expected: String, got: String },
}

/// Result type alias for BOONDB operations
pub type Result<T> = std::result::Result<T, Error>;
