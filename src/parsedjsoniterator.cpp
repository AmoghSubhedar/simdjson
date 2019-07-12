#include "simdjson/parsedjson.h"
#include "simdjson/common_defs.h"
#include <iterator>

namespace simdjson {
ParsedJson::iterator::iterator(ParsedJson &pj_) : pj(pj_), depth(0), location(0), tape_length(0), depthindex(nullptr) {
        if(!pj.isValid()) {
            throw InvalidJSON();
        }
        depthindex = new scopeindex_t[pj.depthcapacity];
        // memory allocation would throw
        //if(depthindex == nullptr) { 
        //    return;
        //}
        depthindex[0].start_of_scope = location;
        current_val = pj.tape[location++];
        current_type = (current_val >> 56);
        depthindex[0].scope_type = current_type;
        if (current_type == 'r') {
            tape_length = current_val & JSONVALUEMASK;
            if(location < tape_length) {
                current_val = pj.tape[location];
                current_type = (current_val >> 56);
                depth++;
                depthindex[depth].start_of_scope = location;
                depthindex[depth].scope_type = current_type;
              }
        } else {
            // should never happen
            throw InvalidJSON();
        }
}

ParsedJson::iterator::~iterator() {
      delete[] depthindex;
}

ParsedJson::iterator::iterator(const iterator &o):
    pj(o.pj), depth(o.depth), location(o.location),
    tape_length(0), current_type(o.current_type),
    current_val(o.current_val), depthindex(nullptr) {
    depthindex = new scopeindex_t[pj.depthcapacity];
    // allocation might throw
    memcpy(depthindex, o.depthindex, pj.depthcapacity * sizeof(depthindex[0]));
    tape_length = o.tape_length;
}

ParsedJson::iterator::iterator(iterator &&o):
      pj(o.pj), depth(o.depth), location(o.location),
      tape_length(o.tape_length), current_type(o.current_type),
      current_val(o.current_val), depthindex(o.depthindex) {
        o.depthindex = nullptr;// we take ownership
}

bool ParsedJson::iterator::print(std::ostream &os, bool escape_strings) const {
    if(!isOk()) { 
      return false;
    }
    switch (current_type) {
    case '"': // we have a string
    os << '"';
    if(escape_strings) {
        print_with_escapes(get_string(), os, get_string_length());
    } else {
        // was: os << get_string();, but given that we can include null chars, we have to do something crazier:
        std::copy(get_string(), get_string() + get_string_length(), std::ostream_iterator<char>(os));
    }
    os << '"';
    break;
    case 'l': // we have a long int
    os << get_integer();
    break;
    case 'd':
    os << get_double();
    break;
    case 'n': // we have a null
    os << "null";
    break;
    case 't': // we have a true
    os << "true";
    break;
    case 'f': // we have a false
    os << "false";
    break;
    case '{': // we have an object
    case '}': // we end an object
    case '[': // we start an array
    case ']': // we end an array
    os << static_cast<char>(current_type);
    break;
    default:
    return false;
    }
    return true;
}

bool ParsedJson::iterator::move_to(const char * pointer, uint32_t length) {
    char* new_pointer = nullptr;
    if (pointer[0] == '#') {
      // Converting fragment representation to string representation
      new_pointer = new char[length];
      uint32_t new_length = 0;
      for (uint32_t i = 1; i < length; i++) {
        if (pointer[i] == '%') {
          try {
            int fragment = std::stoi(std::string(&pointer[i+1], 2), nullptr, 16);
            if (fragment == '\\' || fragment == '"' || (fragment <= 0x1F)) {
              // escaping the character
              new_pointer[new_length] = '\\';
              new_length++;
            }
            new_pointer[new_length] = fragment;
            i += 2;
          }
          catch(std::invalid_argument& e) {
            delete[] new_pointer;
            return false; // the fragment is invalid
          }
        }
        else {
          new_pointer[new_length] = pointer[i];
        }
        new_length++;
      }
      length = new_length;
      pointer = new_pointer;
    }
    std::cout << pointer << " -----------------" << std::endl;
    // saving the current state
    size_t depth_s = depth;
    size_t location_s = location;
    size_t tape_length_s = tape_length;
    uint8_t current_type_s = current_type;
    uint64_t current_val_s = current_val;
    scopeindex_t *depthindex_s = depthindex;
    
    rewind(); // The json pointer is used from the root of the document.

    bool found = relative_move_to(pointer, length);
    delete[] new_pointer;

    if (!found) {
      // since the pointer has found nothing, we get back to the original position.
      depth = depth_s;
      location = location_s;
      tape_length = tape_length_s;
      current_type = current_type_s;
      current_val = current_val_s;
      depthindex = depthindex_s;
    }

    return found;
}

bool ParsedJson::iterator::relative_move_to(const char * pointer, uint32_t length) {
    if (length == 0) {
      return true;
    }
    if (pointer[0] != '/') {
      // '/' must be the first character
      return false;
    }

    // finding the key in an object or the index in an array
    std::string key_or_index;
    uint32_t offset = 1;

    if (is_array() && pointer[1] == '-') {
      if (length != 2) {
        // there can't be anything more after '-' as an index
        return false;
      }
      key_or_index = '-';
      offset = length; // will skip the loop
    }

    for (; offset < length ; offset++) {
      if (pointer[offset] == '/') {
        // beginning of the next key or index
        break;
      }
      if (is_array() && (pointer[offset] < '0' || pointer[offset] > '9')) {
        // the index of an array must be an integer
        // we also make sure std::stoi won't discard whitespaces later
        return false;
      }
      if (pointer[offset] == '~') {
        // "~1" represents "/"
        if (pointer[offset+1] == '1') {
          key_or_index += '/';
          offset++;
          continue;
        }
        // "~0" represents "~"
        if (pointer[offset+1] == '0') {
          key_or_index += '~';
          offset++;
          continue;
        }
      }
      if (pointer[offset] == '\\') {
        if (pointer[offset+1] == '\\' || pointer[offset+1] == '"' || (pointer[offset+1] <= 0x1F)) {
          key_or_index += pointer[offset+1];
          offset++;
          continue;
        }
        else {
          return false; // invalid escaped character
        }
      }
      key_or_index += pointer[offset];
    }
    
    bool found = false;
    if (is_object()) {
      if (move_to_key(key_or_index.c_str(), key_or_index.length())) {
        found = relative_move_to(pointer+offset, length-offset);
      }
    }
    else if(is_array()) {
      if (down()) {
        if (key_or_index == "-") {
          while(next()); // moving to the end of the array
          return true;
        }
        // we already checked the index contains only valid digits
        uint32_t index = std::stoi(key_or_index);
        uint32_t i = 0;
        for (; i < index; i++) {
          if (!next()) {
            break;
          }
        }
        if (i == index) {
          found = relative_move_to(pointer+offset, length-offset);
        }
      }
    }

    return found;
}
}
