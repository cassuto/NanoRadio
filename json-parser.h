/*
 *  NanoRadio (Open source IoT hardware)
 *
 *  This project is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public License(GPL)
 *  as published by the Free Software Foundation; either version 2.1
 *  of the License, or (at your option) any later version.
 *
 *  This project is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 */
/*
  Copyright (c) 2009 Dave Gamble

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
  THE SOFTWARE.
*/

#ifndef JSON_PARSER_H_
#define JSON_PARSER_H_

#ifdef __cplusplus
extern "C"
{
#endif

/* JSON Types: */
#define JSON_False 0
#define JSON_True 1
#define JSON_NULL 2
#define JSON_Number 3
#define JSON_String 4
#define JSON_Array 5
#define JSON_Object 6

#define JSON_IsReference 256

/* The JSON structure: */
typedef struct JSON {
    struct JSON *next, *prev;  /* next/prev allow you to walk array/object chains. Alternatively, use GetArraySize/GetArrayItem/GetObjectItem */
    struct JSON *child;        /* An array or object item will have a child pointer pointing to a chain of the items in the array/object. */

    int type;                   /* The type of the item, as above. */

    char *valuestring;          /* The item's string, if type==JSON_String */
    int valueint;               /* The item's number, if type==JSON_Number */
    double valuedouble;         /* The item's number, if type==JSON_Number */

    char *string;               /* The item's name string, if this item is the child of, or is in the list of subitems of an object. */
} JSON;

/* Supply a block of JSON, and this returns a JSON object you can interrogate. Call JSON_Delete when finished. */
JSON *json_parse(const char *value);
/* Render a JSON entity to text for transfer/storage. Free the char* when finished. */
char  *json_print(JSON *item);
/* Render a JSON entity to text for transfer/storage without any formatting. Free the char* when finished. */
char  *json_print_unformatted(JSON *item);
/* Delete a JSON entity and all subentities. */
void   json_delete(JSON *c);

/* Returns the number of items in an array (or object). */
int    json_get_array_size(JSON *array);
/* Retrieve item number "item" from array "array". Returns NULL if unsuccessful. */
JSON *json_get_array_item(JSON *array, int item);
/* Get item "string" from object. Case insensitive. */
JSON *json_get_object_item(JSON *object, const char *string);

/* For analysing failed parses. This returns a pointer to the parse error. You'll probably need to look a few chars back to make sense of it. Defined when JSON_Parse() returns 0. 0 when JSON_Parse() succeeds. */
const char *json_get_error_ptr(void);

/* These calls create a JSON item of the appropriate type. */
JSON *json_create_null(void);
JSON *json_create_true(void);
JSON *json_create_false(void);
JSON *json_create_bool(int b);
JSON *json_create_number(double num);
JSON *json_create_string(const char *string);
JSON *json_create_array(void);
JSON *json_create_object(void);

/* These utilities create an Array of count items. */
JSON *json_create_int_array(const int *numbers, int count);
JSON *json_create_float_array(const float *numbers, int count);
JSON *json_create_double_array(const double *numbers, int count);
JSON *json_create_string_array(const char **strings, int count);

/* Append item to the specified array/object. */
void json_add_item_to_array(JSON *array, JSON *item);
void json_add_item_to_object(JSON *object, const char *string, JSON *item);
/* Append reference to item to the specified array/object. Use this when you want to add an existing JSON to a new JSON, but don't want to corrupt your existing JSON. */
void json_add_item_reference_to_array(JSON *array, JSON *item);
void jspn_add_item_reference_to_object(JSON *object, const char *string, JSON *item);

/* Remove/Detatch items from Arrays/Objects. */
JSON *json_detach_item_from_array(JSON *array, int which);
void   json_delete_item_from_array(JSON *array, int which);
JSON *json_detach_item_from_object(JSON *object, const char *string);
void   json_delete_item_from_object(JSON *object, const char *string);

/* Update array items. */
void json_replace_item_inarray(JSON *array, int which, JSON *newitem);
void json_replace_item_inobject(JSON *object, const char *string, JSON *newitem);

/* Duplicate a JSON item */
JSON *json_duplicate(JSON *item, int recurse);
/* Duplicate will create a new, identical JSON item to the one you pass, in new memory that will
need to be released. With recurse!=0, it will duplicate any children connected to the item.
The item->next and ->prev pointers are always zero on return from Duplicate. */

/* ParseWithOpts allows you to require (and check) that the JSON is null terminated, and to retrieve the pointer to the final byte parsed. */
JSON *json_parse_with_opts(const char *value, const char **return_parse_end, int require_null_terminated);

void json_minify(char *json);

/* Macros for creating things quickly. */
#define JSON_AddNullToObject(object,name)      JSON_AddItemToObject(object, name, JSON_CreateNull())
#define JSON_AddTrueToObject(object,name)      JSON_AddItemToObject(object, name, JSON_CreateTrue())
#define JSON_AddFalseToObject(object,name)     JSON_AddItemToObject(object, name, JSON_CreateFalse())
#define JSON_AddBoolToObject(object,name,b)    JSON_AddItemToObject(object, name, JSON_CreateBool(b))
#define JSON_AddNumberToObject(object,name,n)  JSON_AddItemToObject(object, name, JSON_CreateNumber(n))
#define JSON_AddStringToObject(object,name,s)  JSON_AddItemToObject(object, name, JSON_CreateString(s))

/* When assigning an integer value, it needs to be propagated to valuedouble too. */
#define JSON_SetIntValue(object,val)           ((object)?(object)->valueint=(object)->valuedouble=(val):(val))

#ifdef __cplusplus
}
#endif

#endif // !defined(JSON_PARSER_H_)
