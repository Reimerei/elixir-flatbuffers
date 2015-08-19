/*
 * Copyright 2014 Google Inc. All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <algorithm>
#include <list>

#include "flatbuffers/idl.h"
#include "flatbuffers/util.h"

namespace flatbuffers {

const char *const kTypeNames[] = {
  #define FLATBUFFERS_TD(ENUM, IDLTYPE, CTYPE, JTYPE, GTYPE, NTYPE, PTYPE) \
    IDLTYPE,
    FLATBUFFERS_GEN_TYPES(FLATBUFFERS_TD)
  #undef FLATBUFFERS_TD
  nullptr
};

const char kTypeSizes[] = {
  #define FLATBUFFERS_TD(ENUM, IDLTYPE, CTYPE, JTYPE, GTYPE, NTYPE, PTYPE) \
      sizeof(CTYPE),
    FLATBUFFERS_GEN_TYPES(FLATBUFFERS_TD)
  #undef FLATBUFFERS_TD
};

// The enums in the reflection schema should match the ones we use internally.
// Compare the last element to check if these go out of sync.
static_assert(BASE_TYPE_UNION ==
              static_cast<BaseType>(reflection::Union),
              "enums don't match");

static void Error(const std::string &msg) {
  throw msg;
}

// Ensure that integer values we parse fit inside the declared integer type.
static void CheckBitsFit(int64_t val, size_t bits) {
  auto mask = (1ll << bits) - 1;  // Bits we allow to be used.
  if (bits < 64 &&
      (val & ~mask) != 0 &&  // Positive or unsigned.
      (val |  mask) != -1)   // Negative.
    Error("constant does not fit in a " + NumToString(bits) + "-bit field");
}

// atot: templated version of atoi/atof: convert a string to an instance of T.
template<typename T> inline T atot(const char *s) {
  auto val = StringToInt(s);
  CheckBitsFit(val, sizeof(T) * 8);
  return (T)val;
}
template<> inline bool atot<bool>(const char *s) {
  return 0 != atoi(s);
}
template<> inline float atot<float>(const char *s) {
  return static_cast<float>(strtod(s, nullptr));
}
template<> inline double atot<double>(const char *s) {
  return strtod(s, nullptr);
}

template<> inline Offset<void> atot<Offset<void>>(const char *s) {
  return Offset<void>(atoi(s));
}

// Declare tokens we'll use. Single character tokens are represented by their
// ascii character code (e.g. '{'), others above 256.
#define FLATBUFFERS_GEN_TOKENS(TD) \
  TD(Eof, 256, "end of file") \
  TD(StringConstant, 257, "string constant") \
  TD(IntegerConstant, 258, "integer constant") \
  TD(FloatConstant, 259, "float constant") \
  TD(Identifier, 260, "identifier") \
  TD(Table, 261, "table") \
  TD(Struct, 262, "struct") \
  TD(Enum, 263, "enum") \
  TD(Union, 264, "union") \
  TD(NameSpace, 265, "namespace") \
  TD(RootType, 266, "root_type") \
  TD(FileIdentifier, 267, "file_identifier") \
  TD(FileExtension, 268, "file_extension") \
  TD(Include, 269, "include") \
  TD(Attribute, 270, "attribute")
#ifdef __GNUC__
__extension__  // Stop GCC complaining about trailing comma with -Wpendantic.
#endif
enum {
  #define FLATBUFFERS_TOKEN(NAME, VALUE, STRING) kToken ## NAME = VALUE,
    FLATBUFFERS_GEN_TOKENS(FLATBUFFERS_TOKEN)
  #undef FLATBUFFERS_TOKEN
  #define FLATBUFFERS_TD(ENUM, IDLTYPE, CTYPE, JTYPE, GTYPE, NTYPE, PTYPE) \
      kToken ## ENUM,
    FLATBUFFERS_GEN_TYPES(FLATBUFFERS_TD)
  #undef FLATBUFFERS_TD
};

static std::string TokenToString(int t) {
  static const char *tokens[] = {
    #define FLATBUFFERS_TOKEN(NAME, VALUE, STRING) STRING,
      FLATBUFFERS_GEN_TOKENS(FLATBUFFERS_TOKEN)
    #undef FLATBUFFERS_TOKEN
    #define FLATBUFFERS_TD(ENUM, IDLTYPE, CTYPE, JTYPE, GTYPE, NTYPE, PTYPE) \
      IDLTYPE,
      FLATBUFFERS_GEN_TYPES(FLATBUFFERS_TD)
    #undef FLATBUFFERS_TD
  };
  if (t < 256) {  // A single ascii char token.
    std::string s;
    s.append(1, static_cast<char>(t));
    return s;
  } else {       // Other tokens.
    return tokens[t - 256];
  }
}

// Parses exactly nibbles worth of hex digits into a number, or error.
int64_t Parser::ParseHexNum(int nibbles) {
  for (int i = 0; i < nibbles; i++)
    if (!isxdigit(cursor_[i]))
      Error("escape code must be followed by " + NumToString(nibbles) +
            " hex digits");
  std::string target(cursor_, cursor_ + nibbles);
  auto val = StringToInt(target.c_str(), 16);
  cursor_ += nibbles;
  return val;
}

void Parser::Next() {
  doc_comment_.clear();
  bool seen_newline = false;
  for (;;) {
    char c = *cursor_++;
    token_ = c;
    switch (c) {
      case '\0': cursor_--; token_ = kTokenEof; return;
      case ' ': case '\r': case '\t': break;
      case '\n': line_++; seen_newline = true; break;
      case '{': case '}': case '(': case ')': case '[': case ']': return;
      case ',': case ':': case ';': case '=': return;
      case '.':
        if(!isdigit(*cursor_)) return;
        Error("floating point constant can\'t start with \".\"");
        break;
      case '\"':
        attribute_ = "";
        while (*cursor_ != '\"') {
          if (*cursor_ < ' ' && *cursor_ >= 0)
            Error("illegal character in string constant");
          if (*cursor_ == '\\') {
            cursor_++;
            switch (*cursor_) {
              case 'n':  attribute_ += '\n'; cursor_++; break;
              case 't':  attribute_ += '\t'; cursor_++; break;
              case 'r':  attribute_ += '\r'; cursor_++; break;
              case 'b':  attribute_ += '\b'; cursor_++; break;
              case 'f':  attribute_ += '\f'; cursor_++; break;
              case '\"': attribute_ += '\"'; cursor_++; break;
              case '\\': attribute_ += '\\'; cursor_++; break;
              case '/':  attribute_ += '/';  cursor_++; break;
              case 'x': {  // Not in the JSON standard
                cursor_++;
                attribute_ += static_cast<char>(ParseHexNum(2));
                break;
              }
              case 'u': {
                cursor_++;
                ToUTF8(static_cast<int>(ParseHexNum(4)), &attribute_);
                break;
              }
              default: Error("unknown escape code in string constant"); break;
            }
          } else { // printable chars + UTF-8 bytes
            attribute_ += *cursor_++;
          }
        }
        cursor_++;
        token_ = kTokenStringConstant;
        return;
      case '/':
        if (*cursor_ == '/') {
          const char *start = ++cursor_;
          while (*cursor_ && *cursor_ != '\n' && *cursor_ != '\r') cursor_++;
          if (*start == '/') {  // documentation comment
            if (cursor_ != source_ && !seen_newline)
              Error("a documentation comment should be on a line on its own");
            doc_comment_.push_back(std::string(start + 1, cursor_));
          }
          break;
        }
        // fall thru
      default:
        if (isalpha(static_cast<unsigned char>(c)) || c == '_') {
          // Collect all chars of an identifier:
          const char *start = cursor_ - 1;
          while (isalnum(static_cast<unsigned char>(*cursor_)) ||
                 *cursor_ == '_')
            cursor_++;
          attribute_.clear();
          attribute_.append(start, cursor_);
          // First, see if it is a type keyword from the table of types:
          #define FLATBUFFERS_TD(ENUM, IDLTYPE, CTYPE, JTYPE, GTYPE, NTYPE, \
            PTYPE) \
            if (attribute_ == IDLTYPE) { \
              token_ = kToken ## ENUM; \
              return; \
            }
            FLATBUFFERS_GEN_TYPES(FLATBUFFERS_TD)
          #undef FLATBUFFERS_TD
          // If it's a boolean constant keyword, turn those into integers,
          // which simplifies our logic downstream.
          if (attribute_ == "true" || attribute_ == "false") {
            attribute_ = NumToString(attribute_ == "true");
            token_ = kTokenIntegerConstant;
            return;
          }
          // Check for declaration keywords:
          if (attribute_ == "table")     { token_ = kTokenTable;     return; }
          if (attribute_ == "struct")    { token_ = kTokenStruct;    return; }
          if (attribute_ == "enum")      { token_ = kTokenEnum;      return; }
          if (attribute_ == "union")     { token_ = kTokenUnion;     return; }
          if (attribute_ == "namespace") { token_ = kTokenNameSpace; return; }
          if (attribute_ == "root_type") { token_ = kTokenRootType;  return; }
          if (attribute_ == "include")   { token_ = kTokenInclude;   return; }
          if (attribute_ == "attribute") { token_ = kTokenAttribute; return; }
          if (attribute_ == "file_identifier") {
            token_ = kTokenFileIdentifier;
            return;
          }
          if (attribute_ == "file_extension") {
            token_ = kTokenFileExtension;
            return;
          }
          // If not, it is a user-defined identifier:
          token_ = kTokenIdentifier;
          return;
        } else if (isdigit(static_cast<unsigned char>(c)) || c == '-') {
          const char *start = cursor_ - 1;
          while (isdigit(static_cast<unsigned char>(*cursor_))) cursor_++;
          if (*cursor_ == '.') {
            cursor_++;
            while (isdigit(static_cast<unsigned char>(*cursor_))) cursor_++;
            // See if this float has a scientific notation suffix. Both JSON
            // and C++ (through strtod() we use) have the same format:
            if (*cursor_ == 'e' || *cursor_ == 'E') {
              cursor_++;
              if (*cursor_ == '+' || *cursor_ == '-') cursor_++;
              while (isdigit(static_cast<unsigned char>(*cursor_))) cursor_++;
            }
            token_ = kTokenFloatConstant;
          } else {
            token_ = kTokenIntegerConstant;
          }
          attribute_.clear();
          attribute_.append(start, cursor_);
          return;
        }
        std::string ch;
        ch = c;
        if (c < ' ' || c > '~') ch = "code: " + NumToString(c);
        Error("illegal character: " + ch);
        break;
    }
  }
}

// Check if a given token is next, if so, consume it as well.
bool Parser::IsNext(int t) {
  bool isnext = t == token_;
  if (isnext) Next();
  return isnext;
}

// Expect a given token to be next, consume it, or error if not present.
void Parser::Expect(int t) {
  if (t != token_) {
    Error("expecting: " + TokenToString(t) + " instead got: " +
          TokenToString(token_));
  }
  Next();
}

void Parser::ParseNamespacing(std::string *id, std::string *last) {
  while (IsNext('.')) {
    *id += ".";
    *id += attribute_;
    if (last) *last = attribute_;
    Expect(kTokenIdentifier);
  }
}

EnumDef *Parser::LookupEnum(const std::string &id) {
  auto ed = enums_.Lookup(GetFullyQualifiedName(id));
  // id may simply not have a namespace at all, so check that too.
  if (!ed) ed = enums_.Lookup(id);
  return ed;
}

void Parser::ParseTypeIdent(Type &type) {
  std::string id = attribute_;
  Expect(kTokenIdentifier);
  ParseNamespacing(&id, nullptr);
  auto enum_def = LookupEnum(id);
  if (enum_def) {
    type = enum_def->underlying_type;
    if (enum_def->is_union) type.base_type = BASE_TYPE_UNION;
  } else {
    type.base_type = BASE_TYPE_STRUCT;
    type.struct_def = LookupCreateStruct(id);
  }
}

// Parse any IDL type.
void Parser::ParseType(Type &type) {
  if (token_ >= kTokenBOOL && token_ <= kTokenSTRING) {
    type.base_type = static_cast<BaseType>(token_ - kTokenNONE);
    Next();
  } else {
    if (token_ == kTokenIdentifier) {
      ParseTypeIdent(type);
    } else if (token_ == '[') {
      Next();
      Type subtype;
      ParseType(subtype);
      if (subtype.base_type == BASE_TYPE_VECTOR) {
        // We could support this, but it will complicate things, and it's
        // easier to work around with a struct around the inner vector.
        Error("nested vector types not supported (wrap in table first).");
      }
      if (subtype.base_type == BASE_TYPE_UNION) {
        // We could support this if we stored a struct of 2 elements per
        // union element.
        Error("vector of union types not supported (wrap in table first).");
      }
      type = Type(BASE_TYPE_VECTOR, subtype.struct_def, subtype.enum_def);
      type.element = subtype.base_type;
      Expect(']');
    } else {
      Error("illegal type syntax");
    }
  }
}

FieldDef &Parser::AddField(StructDef &struct_def,
                           const std::string &name,
                           const Type &type) {
  auto &field = *new FieldDef();
  field.value.offset =
    FieldIndexToOffset(static_cast<voffset_t>(struct_def.fields.vec.size()));
  field.name = name;
  field.file = struct_def.file;
  field.value.type = type;
  if (struct_def.fixed) {  // statically compute the field offset
    auto size = InlineSize(type);
    auto alignment = InlineAlignment(type);
    // structs_ need to have a predictable format, so we need to align to
    // the largest scalar
    struct_def.minalign = std::max(struct_def.minalign, alignment);
    struct_def.PadLastField(alignment);
    field.value.offset = static_cast<voffset_t>(struct_def.bytesize);
    struct_def.bytesize += size;
  }
  if (struct_def.fields.Add(name, &field))
    Error("field already exists: " + name);
  return field;
}

void Parser::ParseField(StructDef &struct_def) {
  std::string name = attribute_;
  std::vector<std::string> dc = doc_comment_;
  Expect(kTokenIdentifier);
  Expect(':');
  Type type;
  ParseType(type);

  if (struct_def.fixed && !IsScalar(type.base_type) && !IsStruct(type))
    Error("structs_ may contain only scalar or struct fields");

  FieldDef *typefield = nullptr;
  if (type.base_type == BASE_TYPE_UNION) {
    // For union fields, add a second auto-generated field to hold the type,
    // with _type appended as the name.
    typefield = &AddField(struct_def, name + "_type",
                          type.enum_def->underlying_type);
  }

  auto &field = AddField(struct_def, name, type);

  if (token_ == '=') {
    Next();
    if (!IsScalar(type.base_type))
      Error("default values currently only supported for scalars");
    ParseSingleValue(field.value);
  }

  if (type.enum_def &&
      IsScalar(type.base_type) &&
      !struct_def.fixed &&
      !type.enum_def->attributes.Lookup("bit_flags") &&
      !type.enum_def->ReverseLookup(static_cast<int>(
                         StringToInt(field.value.constant.c_str()))))
    Error("enum " + type.enum_def->name +
          " does not have a declaration for this field\'s default of " +
          field.value.constant);

  field.doc_comment = dc;
  ParseMetaData(field);
  field.deprecated = field.attributes.Lookup("deprecated") != nullptr;
  auto hash_name = field.attributes.Lookup("hash");
  if (hash_name) {
    switch (type.base_type) {
      case BASE_TYPE_INT:
      case BASE_TYPE_UINT: {
        if (FindHashFunction32(hash_name->constant.c_str()) == nullptr)
          Error("Unknown hashing algorithm for 32 bit types: " +
                hash_name->constant);
        break;
      }
      case BASE_TYPE_LONG:
      case BASE_TYPE_ULONG: {
        if (FindHashFunction64(hash_name->constant.c_str()) == nullptr)
          Error("Unknown hashing algorithm for 64 bit types: " +
                hash_name->constant);
        break;
      }
      default:
        Error("only int, uint, long and ulong data types support hashing.");
    }
  }
  if (field.deprecated && struct_def.fixed)
    Error("can't deprecate fields in a struct");
  field.required = field.attributes.Lookup("required") != nullptr;
  if (field.required && (struct_def.fixed ||
                         IsScalar(field.value.type.base_type)))
    Error("only non-scalar fields in tables may be 'required'");
  field.key = field.attributes.Lookup("key") != nullptr;
  if (field.key) {
    if (struct_def.has_key)
      Error("only one field may be set as 'key'");
    struct_def.has_key = true;
    if (!IsScalar(field.value.type.base_type)) {
      field.required = true;
      if (field.value.type.base_type != BASE_TYPE_STRING)
        Error("'key' field must be string or scalar type");
    }
  }
  auto nested = field.attributes.Lookup("nested_flatbuffer");
  if (nested) {
    if (nested->type.base_type != BASE_TYPE_STRING)
      Error("nested_flatbuffer attribute must be a string (the root type)");
    if (field.value.type.base_type != BASE_TYPE_VECTOR ||
        field.value.type.element != BASE_TYPE_UCHAR)
      Error("nested_flatbuffer attribute may only apply to a vector of ubyte");
    // This will cause an error if the root type of the nested flatbuffer
    // wasn't defined elsewhere.
    LookupCreateStruct(nested->constant);
  }

  if (typefield) {
    // If this field is a union, and it has a manually assigned id,
    // the automatically added type field should have an id as well (of N - 1).
    auto attr = field.attributes.Lookup("id");
    if (attr) {
      auto id = atoi(attr->constant.c_str());
      auto val = new Value();
      val->type = attr->type;
      val->constant = NumToString(id - 1);
      typefield->attributes.Add("id", val);
    }
  }

  Expect(';');
}

void Parser::ParseAnyValue(Value &val, FieldDef *field) {
  switch (val.type.base_type) {
    case BASE_TYPE_UNION: {
      assert(field);
      if (!field_stack_.size() ||
          field_stack_.back().second->value.type.base_type != BASE_TYPE_UTYPE)
        Error("missing type field before this union value: " + field->name);
      auto enum_idx = atot<unsigned char>(
                                    field_stack_.back().first.constant.c_str());
      auto enum_val = val.type.enum_def->ReverseLookup(enum_idx);
      if (!enum_val) Error("illegal type id for: " + field->name);
      val.constant = NumToString(ParseTable(*enum_val->struct_def));
      break;
    }
    case BASE_TYPE_STRUCT:
      val.constant = NumToString(ParseTable(*val.type.struct_def));
      break;
    case BASE_TYPE_STRING: {
      auto s = attribute_;
      Expect(kTokenStringConstant);
      val.constant = NumToString(builder_.CreateString(s).o);
      break;
    }
    case BASE_TYPE_VECTOR: {
      Expect('[');
      val.constant = NumToString(ParseVector(val.type.VectorType()));
      break;
    }
    case BASE_TYPE_INT:
    case BASE_TYPE_UINT:
    case BASE_TYPE_LONG:
    case BASE_TYPE_ULONG: {
      if (field && field->attributes.Lookup("hash") &&
          (token_ == kTokenIdentifier || token_ == kTokenStringConstant)) {
        ParseHash(val, field);
      } else {
        ParseSingleValue(val);
      }
      break;
    }
    default:
      ParseSingleValue(val);
      break;
  }
}

void Parser::SerializeStruct(const StructDef &struct_def, const Value &val) {
  auto off = atot<uoffset_t>(val.constant.c_str());
  assert(struct_stack_.size() - off == struct_def.bytesize);
  builder_.Align(struct_def.minalign);
  builder_.PushBytes(&struct_stack_[off], struct_def.bytesize);
  struct_stack_.resize(struct_stack_.size() - struct_def.bytesize);
  builder_.AddStructOffset(val.offset, builder_.GetSize());
}

uoffset_t Parser::ParseTable(const StructDef &struct_def) {
  Expect('{');
  size_t fieldn = 0;
  for (;;) {
    if ((!strict_json_ || !fieldn) && IsNext('}')) break;
    std::string name = attribute_;
    if (!IsNext(kTokenStringConstant))
      Expect(strict_json_ ? kTokenStringConstant : kTokenIdentifier);
    auto field = struct_def.fields.Lookup(name);
    if (!field) Error("unknown field: " + name);
    if (struct_def.fixed && (fieldn >= struct_def.fields.vec.size()
                            || struct_def.fields.vec[fieldn] != field)) {
       Error("struct field appearing out of order: " + name);
    }
    Expect(':');
    Value val = field->value;
    ParseAnyValue(val, field);
    field_stack_.push_back(std::make_pair(val, field));
    fieldn++;
    if (IsNext('}')) break;
    Expect(',');
  }
  for (auto it = field_stack_.rbegin();
           it != field_stack_.rbegin() + fieldn; ++it) {
    if (it->second->used)
      Error("field set more than once: " + it->second->name);
    it->second->used = true;
  }
  for (auto it = field_stack_.rbegin();
           it != field_stack_.rbegin() + fieldn; ++it) {
    it->second->used = false;
  }
  if (struct_def.fixed && fieldn != struct_def.fields.vec.size())
    Error("incomplete struct initialization: " + struct_def.name);
  auto start = struct_def.fixed
                 ? builder_.StartStruct(struct_def.minalign)
                 : builder_.StartTable();

  for (size_t size = struct_def.sortbysize ? sizeof(largest_scalar_t) : 1;
       size;
       size /= 2) {
    // Go through elements in reverse, since we're building the data backwards.
    for (auto it = field_stack_.rbegin();
             it != field_stack_.rbegin() + fieldn; ++it) {
      auto &value = it->first;
      auto field = it->second;
      if (!struct_def.sortbysize || size == SizeOf(value.type.base_type)) {
        switch (value.type.base_type) {
          #define FLATBUFFERS_TD(ENUM, IDLTYPE, CTYPE, JTYPE, GTYPE, NTYPE, \
            PTYPE) \
            case BASE_TYPE_ ## ENUM: \
              builder_.Pad(field->padding); \
              if (struct_def.fixed) { \
                builder_.PushElement(atot<CTYPE>(value.constant.c_str())); \
              } else { \
                builder_.AddElement(value.offset, \
                             atot<CTYPE>(       value.constant.c_str()), \
                             atot<CTYPE>(field->value.constant.c_str())); \
              } \
              break;
            FLATBUFFERS_GEN_TYPES_SCALAR(FLATBUFFERS_TD);
          #undef FLATBUFFERS_TD
          #define FLATBUFFERS_TD(ENUM, IDLTYPE, CTYPE, JTYPE, GTYPE, NTYPE, \
            PTYPE) \
            case BASE_TYPE_ ## ENUM: \
              builder_.Pad(field->padding); \
              if (IsStruct(field->value.type)) { \
                SerializeStruct(*field->value.type.struct_def, value); \
              } else { \
                builder_.AddOffset(value.offset, \
                  atot<CTYPE>(value.constant.c_str())); \
              } \
              break;
            FLATBUFFERS_GEN_TYPES_POINTER(FLATBUFFERS_TD);
          #undef FLATBUFFERS_TD
        }
      }
    }
  }
  for (size_t i = 0; i < fieldn; i++) field_stack_.pop_back();

  if (struct_def.fixed) {
    builder_.ClearOffsets();
    builder_.EndStruct();
    // Temporarily store this struct in a side buffer, since this data has to
    // be stored in-line later in the parent object.
    auto off = struct_stack_.size();
    struct_stack_.insert(struct_stack_.end(),
                         builder_.GetBufferPointer(),
                         builder_.GetBufferPointer() + struct_def.bytesize);
    builder_.PopBytes(struct_def.bytesize);
    return static_cast<uoffset_t>(off);
  } else {
    return builder_.EndTable(
      start,
      static_cast<voffset_t>(struct_def.fields.vec.size()));
  }
}

uoffset_t Parser::ParseVector(const Type &type) {
  int count = 0;
  for (;;) {
    if ((!strict_json_ || !count) && IsNext(']')) break;
    Value val;
    val.type = type;
    ParseAnyValue(val, nullptr);
    field_stack_.push_back(std::make_pair(val, nullptr));
    count++;
    if (IsNext(']')) break;
    Expect(',');
  }

  builder_.StartVector(count * InlineSize(type) / InlineAlignment(type),
                       InlineAlignment(type));
  for (int i = 0; i < count; i++) {
    // start at the back, since we're building the data backwards.
    auto &val = field_stack_.back().first;
    switch (val.type.base_type) {
      #define FLATBUFFERS_TD(ENUM, IDLTYPE, CTYPE, JTYPE, GTYPE, NTYPE, PTYPE) \
        case BASE_TYPE_ ## ENUM: \
          if (IsStruct(val.type)) SerializeStruct(*val.type.struct_def, val); \
          else builder_.PushElement(atot<CTYPE>(val.constant.c_str())); \
          break;
        FLATBUFFERS_GEN_TYPES(FLATBUFFERS_TD)
      #undef FLATBUFFERS_TD
    }
    field_stack_.pop_back();
  }

  builder_.ClearOffsets();
  return builder_.EndVector(count);
}

void Parser::ParseMetaData(Definition &def) {
  if (IsNext('(')) {
    for (;;) {
      auto name = attribute_;
      Expect(kTokenIdentifier);
      if (known_attributes_.find(name) == known_attributes_.end())
        Error("user define attributes must be declared before use: " + name);
      auto e = new Value();
      def.attributes.Add(name, e);
      if (IsNext(':')) {
        ParseSingleValue(*e);
      }
      if (IsNext(')')) break;
      Expect(',');
    }
  }
}

bool Parser::TryTypedValue(int dtoken,
                           bool check,
                           Value &e,
                           BaseType req) {
  bool match = dtoken == token_;
  if (match) {
    e.constant = attribute_;
    if (!check) {
      if (e.type.base_type == BASE_TYPE_NONE) {
        e.type.base_type = req;
      } else {
        Error(std::string("type mismatch: expecting: ") +
              kTypeNames[e.type.base_type] +
              ", found: " +
              kTypeNames[req]);
      }
    }
    Next();
  }
  return match;
}

int64_t Parser::ParseIntegerFromString(Type &type) {
  int64_t result = 0;
  // Parse one or more enum identifiers, separated by spaces.
  const char *next = attribute_.c_str();
  do {
    const char *divider = strchr(next, ' ');
    std::string word;
    if (divider) {
      word = std::string(next, divider);
      next = divider + strspn(divider, " ");
    } else {
      word = next;
      next += word.length();
    }
    if (type.enum_def) {  // The field has an enum type
      auto enum_val = type.enum_def->vals.Lookup(word);
      if (!enum_val)
        Error("unknown enum value: " + word +
              ", for enum: " + type.enum_def->name);
      result |= enum_val->value;
    } else {  // No enum type, probably integral field.
      if (!IsInteger(type.base_type))
        Error("not a valid value for this field: " + word);
      // TODO: could check if its a valid number constant here.
      const char *dot = strrchr(word.c_str(), '.');
      if (!dot) Error("enum values need to be qualified by an enum type");
      std::string enum_def_str(word.c_str(), dot);
      std::string enum_val_str(dot + 1, word.c_str() + word.length());
      auto enum_def = LookupEnum(enum_def_str);
      if (!enum_def) Error("unknown enum: " + enum_def_str);
      auto enum_val = enum_def->vals.Lookup(enum_val_str);
      if (!enum_val) Error("unknown enum value: " + enum_val_str);
      result |= enum_val->value;
    }
  } while(*next);
  return result;
}


void Parser::ParseHash(Value &e, FieldDef* field) {
  assert(field);
  Value *hash_name = field->attributes.Lookup("hash");
  switch (e.type.base_type) {
    case BASE_TYPE_INT:
    case BASE_TYPE_UINT: {
      auto hash = FindHashFunction32(hash_name->constant.c_str());
      uint32_t hashed_value = hash(attribute_.c_str());
      e.constant = NumToString(hashed_value);
      break;
    }
    case BASE_TYPE_LONG:
    case BASE_TYPE_ULONG: {
      auto hash = FindHashFunction64(hash_name->constant.c_str());
      uint64_t hashed_value = hash(attribute_.c_str());
      e.constant = NumToString(hashed_value);
      break;
    }
    default:
      assert(0);
  }
  Next();
}

void Parser::ParseSingleValue(Value &e) {
  // First check if this could be a string/identifier enum value:
  if (e.type.base_type != BASE_TYPE_STRING &&
      e.type.base_type != BASE_TYPE_NONE &&
      (token_ == kTokenIdentifier || token_ == kTokenStringConstant)) {
      e.constant = NumToString(ParseIntegerFromString(e.type));
      Next();
  } else if (TryTypedValue(kTokenIntegerConstant,
                    IsScalar(e.type.base_type),
                    e,
                    BASE_TYPE_INT) ||
      TryTypedValue(kTokenFloatConstant,
                    IsFloat(e.type.base_type),
                    e,
                    BASE_TYPE_FLOAT) ||
      TryTypedValue(kTokenStringConstant,
                    e.type.base_type == BASE_TYPE_STRING,
                    e,
                    BASE_TYPE_STRING)) {
  } else {
    Error("cannot parse value starting with: " + TokenToString(token_));
  }
}

StructDef *Parser::LookupCreateStruct(const std::string &name) {
  std::string qualified_name = GetFullyQualifiedName(name);
  auto struct_def = structs_.Lookup(qualified_name);
  // Unqualified names may simply have no namespace at all, so try that too.
  if (!struct_def) struct_def = structs_.Lookup(name);
  if (!struct_def) {
    // Rather than failing, we create a "pre declared" StructDef, due to
    // circular references, and check for errors at the end of parsing.
    struct_def = new StructDef();
    structs_.Add(qualified_name, struct_def);
    struct_def->name = name;
    struct_def->predecl = true;
    struct_def->defined_namespace = namespaces_.back();
  }
  return struct_def;
}

void Parser::ParseEnum(bool is_union) {
  std::vector<std::string> enum_comment = doc_comment_;
  Next();
  std::string enum_name = attribute_;
  Expect(kTokenIdentifier);
  auto &enum_def = *new EnumDef();
  enum_def.name = enum_name;
  if (!files_being_parsed_.empty()) enum_def.file = files_being_parsed_.top();
  enum_def.doc_comment = enum_comment;
  enum_def.is_union = is_union;
  enum_def.defined_namespace = namespaces_.back();
  if (enums_.Add(GetFullyQualifiedName(enum_name), &enum_def))
    Error("enum already exists: " + enum_name);
  if (is_union) {
    enum_def.underlying_type.base_type = BASE_TYPE_UTYPE;
    enum_def.underlying_type.enum_def = &enum_def;
  } else {
    if (proto_mode_) {
      enum_def.underlying_type.base_type = BASE_TYPE_SHORT;
    } else {
      // Give specialized error message, since this type spec used to
      // be optional in the first FlatBuffers release.
      if (!IsNext(':')) Error("must specify the underlying integer type for this"
                              " enum (e.g. \': short\', which was the default).");
      // Specify the integer type underlying this enum.
      ParseType(enum_def.underlying_type);
      if (!IsInteger(enum_def.underlying_type.base_type))
        Error("underlying enum type must be integral");
    }
    // Make this type refer back to the enum it was derived from.
    enum_def.underlying_type.enum_def = &enum_def;
  }
  ParseMetaData(enum_def);
  Expect('{');
  if (is_union) enum_def.vals.Add("NONE", new EnumVal("NONE", 0));
  do {
    auto value_name = attribute_;
    auto full_name = value_name;
    std::vector<std::string> value_comment = doc_comment_;
    Expect(kTokenIdentifier);
    if (is_union) ParseNamespacing(&full_name, &value_name);
    auto prevsize = enum_def.vals.vec.size();
    auto value = enum_def.vals.vec.size()
      ? enum_def.vals.vec.back()->value + 1
      : 0;
    auto &ev = *new EnumVal(value_name, value);
    if (enum_def.vals.Add(value_name, &ev))
      Error("enum value already exists: " + value_name);
    ev.doc_comment = value_comment;
    if (is_union) {
      ev.struct_def = LookupCreateStruct(full_name);
    }
    if (IsNext('=')) {
      ev.value = atoi(attribute_.c_str());
      Expect(kTokenIntegerConstant);
      if (prevsize && enum_def.vals.vec[prevsize - 1]->value >= ev.value)
        Error("enum values must be specified in ascending order");
    }
  } while (IsNext(proto_mode_ ? ';' : ',') && token_ != '}');
  Expect('}');
  if (enum_def.attributes.Lookup("bit_flags")) {
    for (auto it = enum_def.vals.vec.begin(); it != enum_def.vals.vec.end();
         ++it) {
      if (static_cast<size_t>((*it)->value) >=
           SizeOf(enum_def.underlying_type.base_type) * 8)
        Error("bit flag out of range of underlying integral type");
      (*it)->value = 1LL << (*it)->value;
    }
  }
}

StructDef &Parser::StartStruct() {
  std::string name = attribute_;
  Expect(kTokenIdentifier);
  auto &struct_def = *LookupCreateStruct(name);
  if (!struct_def.predecl) Error("datatype already exists: " + name);
  struct_def.predecl = false;
  struct_def.name = name;
  if (!files_being_parsed_.empty()) struct_def.file = files_being_parsed_.top();
  // Move this struct to the back of the vector just in case it was predeclared,
  // to preserve declaration order.
  *remove(structs_.vec.begin(), structs_.vec.end(), &struct_def) = &struct_def;
  return struct_def;
}

void Parser::ParseDecl() {
  std::vector<std::string> dc = doc_comment_;
  bool fixed = IsNext(kTokenStruct);
  if (!fixed) Expect(kTokenTable);
  auto &struct_def = StartStruct();
  struct_def.doc_comment = dc;
  struct_def.fixed = fixed;
  ParseMetaData(struct_def);
  struct_def.sortbysize =
    struct_def.attributes.Lookup("original_order") == nullptr && !fixed;
  Expect('{');
  while (token_ != '}') ParseField(struct_def);
  auto force_align = struct_def.attributes.Lookup("force_align");
  if (fixed && force_align) {
    auto align = static_cast<size_t>(atoi(force_align->constant.c_str()));
    if (force_align->type.base_type != BASE_TYPE_INT ||
        align < struct_def.minalign ||
        align > 16 ||
        align & (align - 1))
      Error("force_align must be a power of two integer ranging from the"
            "struct\'s natural alignment to 16");
    struct_def.minalign = align;
  }
  struct_def.PadLastField(struct_def.minalign);
  // Check if this is a table that has manual id assignments
  auto &fields = struct_def.fields.vec;
  if (!struct_def.fixed && fields.size()) {
    size_t num_id_fields = 0;
    for (auto it = fields.begin(); it != fields.end(); ++it) {
      if ((*it)->attributes.Lookup("id")) num_id_fields++;
    }
    // If any fields have ids..
    if (num_id_fields) {
      // Then all fields must have them.
      if (num_id_fields != fields.size())
        Error("either all fields or no fields must have an 'id' attribute");
      // Simply sort by id, then the fields are the same as if no ids had
      // been specified.
      std::sort(fields.begin(), fields.end(),
        [](const FieldDef *a, const FieldDef *b) -> bool {
          auto a_id = atoi(a->attributes.Lookup("id")->constant.c_str());
          auto b_id = atoi(b->attributes.Lookup("id")->constant.c_str());
          return a_id < b_id;
      });
      // Verify we have a contiguous set, and reassign vtable offsets.
      for (int i = 0; i < static_cast<int>(fields.size()); i++) {
        if (i != atoi(fields[i]->attributes.Lookup("id")->constant.c_str()))
          Error("field id\'s must be consecutive from 0, id " +
                NumToString(i) + " missing or set twice");
        fields[i]->value.offset = FieldIndexToOffset(static_cast<voffset_t>(i));
      }
    }
  }
  // Check that no identifiers clash with auto generated fields.
  // This is not an ideal situation, but should occur very infrequently,
  // and allows us to keep using very readable names for type & length fields
  // without inducing compile errors.
  auto CheckClash = [&fields, &struct_def](const char *suffix,
                                           BaseType basetype) {
    auto len = strlen(suffix);
    for (auto it = fields.begin(); it != fields.end(); ++it) {
      auto &name = (*it)->name;
      if (name.length() > len &&
          name.compare(name.length() - len, len, suffix) == 0 &&
          (*it)->value.type.base_type != BASE_TYPE_UTYPE) {
        auto field = struct_def.fields.Lookup(
                       name.substr(0, name.length() - len));
        if (field && field->value.type.base_type == basetype)
          Error("Field " + name +
                " would clash with generated functions for field " +
                field->name);
      }
    }
  };
  CheckClash("_type", BASE_TYPE_UNION);
  CheckClash("Type", BASE_TYPE_UNION);
  CheckClash("_length", BASE_TYPE_VECTOR);
  CheckClash("Length", BASE_TYPE_VECTOR);
  CheckClash("_byte_vector", BASE_TYPE_STRING);
  CheckClash("ByteVector", BASE_TYPE_STRING);
  Expect('}');
}

bool Parser::SetRootType(const char *name) {
  root_struct_def_ = structs_.Lookup(GetFullyQualifiedName(name));
  return root_struct_def_ != nullptr;
}

std::string Parser::GetFullyQualifiedName(const std::string &name) const {
  Namespace *ns = namespaces_.back();

  // Early exit if we don't have a defined namespace, or if the name is already
  // partially qualified
  if (ns->components.size() == 0 || name.find(".") != std::string::npos) {
    return name;
  }
  std::stringstream stream;
  for (size_t i = 0; i != ns->components.size(); ++i) {
    if (i != 0) {
      stream << ".";
    }
    stream << ns->components[i];
  }

  stream << "." << name;
  return stream.str();
}

void Parser::MarkGenerated() {
  // Since the Parser object retains definitions across files, we must
  // ensure we only output code for definitions once, in the file they are first
  // declared. This function marks all existing definitions as having already
  // been generated.
  for (auto it = enums_.vec.begin();
           it != enums_.vec.end(); ++it) {
    (*it)->generated = true;
  }
  for (auto it = structs_.vec.begin();
           it != structs_.vec.end(); ++it) {
    (*it)->generated = true;
  }
}

void Parser::ParseNamespace() {
  Next();
  auto ns = new Namespace();
  namespaces_.push_back(ns);
  for (;;) {
    ns->components.push_back(attribute_);
    Expect(kTokenIdentifier);
    if (!IsNext('.')) break;
  }
  Expect(';');
}

// Best effort parsing of .proto declarations, with the aim to turn them
// in the closest corresponding FlatBuffer equivalent.
// We parse everything as identifiers instead of keywords, since we don't
// want protobuf keywords to become invalid identifiers in FlatBuffers.
void Parser::ParseProtoDecl() {
  if (attribute_ == "package") {
    // These are identical in syntax to FlatBuffer's namespace decl.
    ParseNamespace();
  } else if (attribute_ == "message") {
    std::vector<std::string> struct_comment = doc_comment_;
    Next();
    auto &struct_def = StartStruct();
    struct_def.doc_comment = struct_comment;
    Expect('{');
    while (token_ != '}') {
      std::vector<std::string> field_comment = doc_comment_;
      // Parse the qualifier.
      bool required = false;
      bool repeated = false;
      if (attribute_ == "optional") {
        // This is the default.
      } else if (attribute_ == "required") {
        required = true;
      } else if (attribute_ == "repeated") {
        repeated = true;
      } else {
        Error("expecting optional/required/repeated, got: " + attribute_);
      }
      Type type = ParseTypeFromProtoType();
      // Repeated elements get mapped to a vector.
      if (repeated) {
        type.element = type.base_type;
        type.base_type = BASE_TYPE_VECTOR;
      }
      std::string name = attribute_;
      Expect(kTokenIdentifier);
      // Parse the field id. Since we're just translating schemas, not
      // any kind of binary compatibility, we can safely ignore these, and
      // assign our own.
      Expect('=');
      Expect(kTokenIntegerConstant);
      auto &field = AddField(struct_def, name, type);
      field.doc_comment = field_comment;
      field.required = required;
      // See if there's a default specified.
      if (IsNext('[')) {
        if (attribute_ != "default") Error("\'default\' expected");
        Next();
        Expect('=');
        field.value.constant = attribute_;
        Next();
        Expect(']');
      }
      Expect(';');
    }
    Next();
  } else if (attribute_ == "enum") {
    // These are almost the same, just with different terminator:
    ParseEnum(false);
  } else if (attribute_ == "import") {
    Next();
    included_files_[attribute_] = true;
    Expect(kTokenStringConstant);
    Expect(';');
  } else if (attribute_ == "option") {  // Skip these.
    Next();
    Expect(kTokenIdentifier);
    Expect('=');
    Next();  // Any single token.
    Expect(';');
  } else {
    Error("don\'t know how to parse .proto declaration starting with " +
          attribute_);
  }
}

// Parse a protobuf type, and map it to the corresponding FlatBuffer one.
Type Parser::ParseTypeFromProtoType() {
  Expect(kTokenIdentifier);
  struct type_lookup { const char *proto_type; BaseType fb_type; };
  static type_lookup lookup[] = {
    { "float", BASE_TYPE_FLOAT },  { "double", BASE_TYPE_DOUBLE },
    { "int32", BASE_TYPE_INT },    { "int64", BASE_TYPE_LONG },
    { "uint32", BASE_TYPE_UINT },  { "uint64", BASE_TYPE_ULONG },
    { "sint32", BASE_TYPE_INT },   { "sint64", BASE_TYPE_LONG },
    { "fixed32", BASE_TYPE_UINT }, { "fixed64", BASE_TYPE_ULONG },
    { "sfixed32", BASE_TYPE_INT }, { "sfixed64", BASE_TYPE_LONG },
    { "bool", BASE_TYPE_BOOL },
    { "string", BASE_TYPE_STRING },
    { "bytes", BASE_TYPE_STRING },
    { nullptr, BASE_TYPE_NONE }
  };
  Type type;
  for (auto tl = lookup; tl->proto_type; tl++) {
    if (attribute_ == tl->proto_type) {
      type.base_type = tl->fb_type;
      Next();
      return type;
    }
  }
  ParseTypeIdent(type);
  return type;
}

bool Parser::Parse(const char *source, const char **include_paths,
                   const char *source_filename) {
  if (source_filename &&
      included_files_.find(source_filename) == included_files_.end()) {
    included_files_[source_filename] = true;
    files_included_per_file_[source_filename] = std::set<std::string>();
    files_being_parsed_.push(source_filename);
  }
  if (!include_paths) {
    static const char *current_directory[] = { "", nullptr };
    include_paths = current_directory;
  }
  source_ = cursor_ = source;
  line_ = 1;
  error_.clear();
  builder_.Clear();
  try {
    Next();
    // Includes must come first:
    while (IsNext(kTokenInclude)) {
      auto name = attribute_;
      Expect(kTokenStringConstant);
      // Look for the file in include_paths.
      std::string filepath;
      for (auto paths = include_paths; paths && *paths; paths++) {
        filepath = flatbuffers::ConCatPathFileName(*paths, name);
        if(FileExists(filepath.c_str())) break;
      }
      if (filepath.empty())
        Error("unable to locate include file: " + name);
      if (source_filename)
        files_included_per_file_[source_filename].insert(filepath);
      if (included_files_.find(filepath) == included_files_.end()) {
        // We found an include file that we have not parsed yet.
        // Load it and parse it.
        std::string contents;
        if (!LoadFile(filepath.c_str(), true, &contents))
          Error("unable to load include file: " + name);
        if (!Parse(contents.c_str(), include_paths, filepath.c_str())) {
          // Any errors, we're done.
          return false;
        }
        // We do not want to output code for any included files:
        MarkGenerated();
        // This is the easiest way to continue this file after an include:
        // instead of saving and restoring all the state, we simply start the
        // file anew. This will cause it to encounter the same include statement
        // again, but this time it will skip it, because it was entered into
        // included_files_.
        // This is recursive, but only go as deep as the number of include
        // statements.
        return Parse(source, include_paths, source_filename);
      }
      Expect(';');
    }
    // Start with a blank namespace just in case this file doesn't have one.
    namespaces_.push_back(new Namespace());
    // Now parse all other kinds of declarations:
    while (token_ != kTokenEof) {
      if (proto_mode_) {
        ParseProtoDecl();
      } else if (token_ == kTokenNameSpace) {
        ParseNamespace();
      } else if (token_ == '{') {
        if (!root_struct_def_) Error("no root type set to parse json with");
        if (builder_.GetSize()) {
          Error("cannot have more than one json object in a file");
        }
        builder_.Finish(Offset<Table>(ParseTable(*root_struct_def_)),
          file_identifier_.length() ? file_identifier_.c_str() : nullptr);
      } else if (token_ == kTokenEnum) {
        ParseEnum(false);
      } else if (token_ == kTokenUnion) {
        ParseEnum(true);
      } else if (token_ == kTokenRootType) {
        Next();
        auto root_type = attribute_;
        Expect(kTokenIdentifier);
        if (!SetRootType(root_type.c_str()))
          Error("unknown root type: " + root_type);
        if (root_struct_def_->fixed)
          Error("root type must be a table");
        Expect(';');
      } else if (token_ == kTokenFileIdentifier) {
        Next();
        file_identifier_ = attribute_;
        Expect(kTokenStringConstant);
        if (file_identifier_.length() !=
            FlatBufferBuilder::kFileIdentifierLength)
          Error("file_identifier must be exactly " +
                NumToString(FlatBufferBuilder::kFileIdentifierLength) +
                " characters");
        Expect(';');
      } else if (token_ == kTokenFileExtension) {
        Next();
        file_extension_ = attribute_;
        Expect(kTokenStringConstant);
        Expect(';');
      } else if(token_ == kTokenInclude) {
        Error("includes must come before declarations");
      } else if(token_ == kTokenAttribute) {
        Next();
        auto name = attribute_;
        Expect(kTokenStringConstant);
        Expect(';');
        known_attributes_.insert(name);
      } else {
        ParseDecl();
      }
    }
    for (auto it = structs_.vec.begin(); it != structs_.vec.end(); ++it) {
      if ((*it)->predecl)
        Error("type referenced but not defined: " + (*it)->name);
    }
    for (auto it = enums_.vec.begin(); it != enums_.vec.end(); ++it) {
      auto &enum_def = **it;
      if (enum_def.is_union) {
        for (auto val_it = enum_def.vals.vec.begin();
             val_it != enum_def.vals.vec.end();
             ++val_it) {
          auto &val = **val_it;
          if (val.struct_def && val.struct_def->fixed)
            Error("only tables can be union elements: " + val.name);
        }
      }
    }
  } catch (const std::string &msg) {
    error_ = source_filename ? AbsolutePath(source_filename) : "";
    #ifdef _WIN32
      error_ += "(" + NumToString(line_) + ")";  // MSVC alike
    #else
      if (source_filename) error_ += ":";
      error_ += NumToString(line_) + ":0";  // gcc alike
    #endif
    error_ += ": error: " + msg;
    if (source_filename) files_being_parsed_.pop();
    return false;
  }
  if (source_filename) files_being_parsed_.pop();
  assert(!struct_stack_.size());
  return true;
}

std::set<std::string> Parser::GetIncludedFilesRecursive(
    const std::string &file_name) const {
  std::set<std::string> included_files;
  std::list<std::string> to_process;

  if (file_name.empty()) return included_files;
  to_process.push_back(file_name);

  while (!to_process.empty()) {
    std::string current = to_process.front();
    to_process.pop_front();
    included_files.insert(current);

    auto new_files = files_included_per_file_.at(current);
    for (auto it = new_files.begin(); it != new_files.end(); ++it) {
      if (included_files.find(*it) == included_files.end())
        to_process.push_back(*it);
    }
  }

  return included_files;
}

// Schema serialization functionality:

template<typename T> void AssignIndices(const std::vector<T *> &defvec) {
  // Pre-sort these vectors, such that we can set the correct indices for them.
  auto vec = defvec;
  std::sort(vec.begin(), vec.end(),
            [](const T *a, const T *b) { return a->name < b->name; });
  for (int i = 0; i < static_cast<int>(vec.size()); i++) vec[i]->index = i;
}

void Parser::Serialize() {
  builder_.Clear();
  AssignIndices(structs_.vec);
  AssignIndices(enums_.vec);
  std::vector<Offset<reflection::Object>> object_offsets;
  for (auto it = structs_.vec.begin(); it != structs_.vec.end(); ++it) {
    auto offset = (*it)->Serialize(&builder_);
    object_offsets.push_back(offset);
    (*it)->serialized_location = offset.o;
  }
  std::vector<Offset<reflection::Enum>> enum_offsets;
  for (auto it = enums_.vec.begin(); it != enums_.vec.end(); ++it) {
    auto offset = (*it)->Serialize(&builder_);
    enum_offsets.push_back(offset);
    (*it)->serialized_location = offset.o;
  }
  auto schema_offset = reflection::CreateSchema(
                         builder_,
                         builder_.CreateVectorOfSortedTables(&object_offsets),
                         builder_.CreateVectorOfSortedTables(&enum_offsets),
                         builder_.CreateString(file_identifier_),
                         builder_.CreateString(file_extension_),
                         root_struct_def_
                           ? root_struct_def_->serialized_location
                           : 0);
  builder_.Finish(schema_offset, reflection::SchemaIdentifier());
}

Offset<reflection::Object> StructDef::Serialize(FlatBufferBuilder *builder)
                                                                         const {
  std::vector<Offset<reflection::Field>> field_offsets;
  for (auto it = fields.vec.begin(); it != fields.vec.end(); ++it) {
    field_offsets.push_back(
      (*it)->Serialize(builder,
                       static_cast<uint16_t>(it - fields.vec.begin())));
  }
  return reflection::CreateObject(*builder,
                                  builder->CreateString(name),
                                  builder->CreateVectorOfSortedTables(
                                    &field_offsets),
                                  fixed,
                                  static_cast<int>(minalign),
                                  static_cast<int>(bytesize));
}

Offset<reflection::Field> FieldDef::Serialize(FlatBufferBuilder *builder,
                                              uint16_t id) const {
  return reflection::CreateField(*builder,
                                 builder->CreateString(name),
                                 value.type.Serialize(builder),
                                 id,
                                 value.offset,
                                 IsInteger(value.type.base_type)
                                   ? StringToInt(value.constant.c_str())
                                   : 0,
                                 IsFloat(value.type.base_type)
                                   ? strtod(value.constant.c_str(), nullptr)
                                   : 0.0,
                                 deprecated,
                                 required,
                                 key);
  // TODO: value.constant is almost always "0", we could save quite a bit of
  // space by sharing it. Same for common values of value.type.
}

Offset<reflection::Enum> EnumDef::Serialize(FlatBufferBuilder *builder) const {
  std::vector<Offset<reflection::EnumVal>> enumval_offsets;
  for (auto it = vals.vec.begin(); it != vals.vec.end(); ++it) {
    enumval_offsets.push_back((*it)->Serialize(builder));
  }
  return reflection::CreateEnum(*builder,
                                builder->CreateString(name),
                                builder->CreateVector(enumval_offsets),
                                is_union,
                                underlying_type.Serialize(builder));
}

Offset<reflection::EnumVal> EnumVal::Serialize(FlatBufferBuilder *builder) const
                                                                               {
  return reflection::CreateEnumVal(*builder,
                                   builder->CreateString(name),
                                   value,
                                   struct_def
                                     ? struct_def->serialized_location
                                     : 0);
}

Offset<reflection::Type> Type::Serialize(FlatBufferBuilder *builder) const {
  return reflection::CreateType(*builder,
                                static_cast<reflection::BaseType>(base_type),
                                static_cast<reflection::BaseType>(element),
                                struct_def ? struct_def->index :
                                             (enum_def ? enum_def->index : -1));
}

}  // namespace flatbuffers
