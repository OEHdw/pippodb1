//! BOON type definitions
//!
//! BOON supports all standard document types plus UniformArray.

use crate::error::{Error, Result};

/// BOON type codes
///
/// Compatible with BSON where applicable, with extensions for BOON-specific features.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
#[repr(u8)]
pub enum BoonType {
    /// End of object marker (internal use)
    Eoo = 0x00,

    /// 64-bit IEEE 754 floating point
    Double = 0x01,

    /// UTF-8 string
    String = 0x02,

    /// Embedded document
    Document = 0x03,

    /// Array of values
    Array = 0x04,

    /// Binary data
    Binary = 0x05,

    /// Undefined (deprecated, for compatibility)
    Undefined = 0x06,

    /// ObjectId (12 bytes)
    ObjectId = 0x07,

    /// Boolean
    Boolean = 0x08,

    /// UTC datetime (milliseconds since epoch)
    DateTime = 0x09,

    /// Null value
    Null = 0x0A,

    /// Regular expression
    Regex = 0x0B,

    /// JavaScript code
    JavaScript = 0x0D,

    /// 32-bit signed integer
    Int32 = 0x10,

    /// Timestamp (internal MongoDB type)
    Timestamp = 0x11,

    /// 64-bit signed integer
    Int64 = 0x12,

    /// 128-bit decimal floating point
    Decimal128 = 0x13,

    // ========== BOON EXTENSIONS ==========

    /// Uniform array - array of objects with identical schema
    /// This is the key innovation of BOON!
    UniformArray = 0x14,

    /// Compact array - array of primitives without index keys
    CompactArray = 0x15,

    /// Schema reference - reference to externally defined schema
    SchemaRef = 0x16,

    /// Min key (for sorting)
    MinKey = 0xFF,

    /// Max key (for sorting)
    MaxKey = 0x7F,
}

impl BoonType {
    /// Create BoonType from u8 byte
    pub fn from_byte(byte: u8) -> Result<Self> {
        match byte {
            0x00 => Ok(BoonType::Eoo),
            0x01 => Ok(BoonType::Double),
            0x02 => Ok(BoonType::String),
            0x03 => Ok(BoonType::Document),
            0x04 => Ok(BoonType::Array),
            0x05 => Ok(BoonType::Binary),
            0x06 => Ok(BoonType::Undefined),
            0x07 => Ok(BoonType::ObjectId),
            0x08 => Ok(BoonType::Boolean),
            0x09 => Ok(BoonType::DateTime),
            0x0A => Ok(BoonType::Null),
            0x0B => Ok(BoonType::Regex),
            0x0D => Ok(BoonType::JavaScript),
            0x10 => Ok(BoonType::Int32),
            0x11 => Ok(BoonType::Timestamp),
            0x12 => Ok(BoonType::Int64),
            0x13 => Ok(BoonType::Decimal128),
            0x14 => Ok(BoonType::UniformArray),
            0x15 => Ok(BoonType::CompactArray),
            0x16 => Ok(BoonType::SchemaRef),
            0x7F => Ok(BoonType::MaxKey),
            0xFF => Ok(BoonType::MinKey),
            _ => Err(Error::InvalidType(byte)),
        }
    }

    /// Convert to u8 byte
    pub fn to_byte(self) -> u8 {
        self as u8
    }

    /// Check if this type has a fixed size
    pub fn is_fixed_size(self) -> bool {
        matches!(
            self,
            BoonType::Double
                | BoonType::Boolean
                | BoonType::DateTime
                | BoonType::Null
                | BoonType::Int32
                | BoonType::Timestamp
                | BoonType::Int64
                | BoonType::Decimal128
                | BoonType::ObjectId
                | BoonType::MinKey
                | BoonType::MaxKey
        )
    }

    /// Get fixed size in bytes (returns None for variable-size types)
    pub fn fixed_size(self) -> Option<usize> {
        match self {
            BoonType::Double => Some(8),
            BoonType::Boolean => Some(1),
            BoonType::DateTime => Some(8),
            BoonType::Null => Some(0),
            BoonType::Int32 => Some(4),
            BoonType::Timestamp => Some(8),
            BoonType::Int64 => Some(8),
            BoonType::Decimal128 => Some(16),
            BoonType::ObjectId => Some(12),
            BoonType::MinKey => Some(0),
            BoonType::MaxKey => Some(0),
            BoonType::Eoo => Some(0),
            _ => None,
        }
    }

    /// Check if this is a numeric type
    pub fn is_numeric(self) -> bool {
        matches!(
            self,
            BoonType::Double
                | BoonType::Int32
                | BoonType::Int64
                | BoonType::Decimal128
        )
    }

    /// Check if this is a BOON extension type
    pub fn is_boon_extension(self) -> bool {
        matches!(
            self,
            BoonType::UniformArray | BoonType::CompactArray | BoonType::SchemaRef
        )
    }

    /// Get human-readable type name
    pub fn name(self) -> &'static str {
        match self {
            BoonType::Eoo => "eoo",
            BoonType::Double => "double",
            BoonType::String => "string",
            BoonType::Document => "document",
            BoonType::Array => "array",
            BoonType::Binary => "binary",
            BoonType::Undefined => "undefined",
            BoonType::ObjectId => "objectId",
            BoonType::Boolean => "boolean",
            BoonType::DateTime => "datetime",
            BoonType::Null => "null",
            BoonType::Regex => "regex",
            BoonType::JavaScript => "javascript",
            BoonType::Int32 => "int32",
            BoonType::Timestamp => "timestamp",
            BoonType::Int64 => "int64",
            BoonType::Decimal128 => "decimal128",
            BoonType::UniformArray => "uniformArray",
            BoonType::CompactArray => "compactArray",
            BoonType::SchemaRef => "schemaRef",
            BoonType::MinKey => "minKey",
            BoonType::MaxKey => "maxKey",
        }
    }
}

impl std::fmt::Display for BoonType {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.name())
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_type_roundtrip() {
        let types = [
            BoonType::Double,
            BoonType::String,
            BoonType::Document,
            BoonType::Array,
            BoonType::Int32,
            BoonType::Int64,
            BoonType::Boolean,
            BoonType::Null,
            BoonType::UniformArray,
            BoonType::CompactArray,
        ];

        for t in types {
            let byte = t.to_byte();
            let recovered = BoonType::from_byte(byte).unwrap();
            assert_eq!(t, recovered);
        }
    }

    #[test]
    fn test_fixed_sizes() {
        assert_eq!(BoonType::Double.fixed_size(), Some(8));
        assert_eq!(BoonType::Int32.fixed_size(), Some(4));
        assert_eq!(BoonType::Int64.fixed_size(), Some(8));
        assert_eq!(BoonType::Boolean.fixed_size(), Some(1));
        assert_eq!(BoonType::Null.fixed_size(), Some(0));
        assert_eq!(BoonType::String.fixed_size(), None);
        assert_eq!(BoonType::Document.fixed_size(), None);
    }

    #[test]
    fn test_is_numeric() {
        assert!(BoonType::Double.is_numeric());
        assert!(BoonType::Int32.is_numeric());
        assert!(BoonType::Int64.is_numeric());
        assert!(!BoonType::String.is_numeric());
        assert!(!BoonType::Boolean.is_numeric());
    }

    #[test]
    fn test_is_boon_extension() {
        assert!(BoonType::UniformArray.is_boon_extension());
        assert!(BoonType::CompactArray.is_boon_extension());
        assert!(!BoonType::Array.is_boon_extension());
        assert!(!BoonType::Document.is_boon_extension());
    }
}
