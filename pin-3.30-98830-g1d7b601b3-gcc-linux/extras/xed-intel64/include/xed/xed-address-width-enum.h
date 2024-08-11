/* BEGIN_LEGAL 

Copyright (c) 2023 Intel Corporation

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
  
END_LEGAL */
/// @file xed-address-width-enum.h

// This file was automatically generated.
// Do not edit this file.

#if !defined(XED_ADDRESS_WIDTH_ENUM_H)
# define XED_ADDRESS_WIDTH_ENUM_H
#include "xed-common-hdrs.h"
#define XED_ADDRESS_WIDTH_INVALID_DEFINED 1
#define XED_ADDRESS_WIDTH_16b_DEFINED 1
#define XED_ADDRESS_WIDTH_32b_DEFINED 1
#define XED_ADDRESS_WIDTH_64b_DEFINED 1
#define XED_ADDRESS_WIDTH_LAST_DEFINED 1
typedef enum {
  XED_ADDRESS_WIDTH_INVALID=0,
  XED_ADDRESS_WIDTH_16b=2, ///< 16b addressing
  XED_ADDRESS_WIDTH_32b=4, ///< 32b addressing
  XED_ADDRESS_WIDTH_64b=8, ///< 64b addressing
  XED_ADDRESS_WIDTH_LAST
} xed_address_width_enum_t;

/// This converts strings to #xed_address_width_enum_t types.
/// @param s A C-string.
/// @return #xed_address_width_enum_t
/// @ingroup ENUM
XED_DLL_EXPORT xed_address_width_enum_t str2xed_address_width_enum_t(const char* s);
/// This converts strings to #xed_address_width_enum_t types.
/// @param p An enumeration element of type xed_address_width_enum_t.
/// @return string
/// @ingroup ENUM
XED_DLL_EXPORT const char* xed_address_width_enum_t2str(const xed_address_width_enum_t p);

/// Returns the last element of the enumeration
/// @return xed_address_width_enum_t The last element of the enumeration.
/// @ingroup ENUM
XED_DLL_EXPORT xed_address_width_enum_t xed_address_width_enum_t_last(void);
#endif
