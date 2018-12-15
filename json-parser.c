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

/* JSON */
/* JSON parser in C. */
#include <math.h>
#include <float.h>
#include <limits.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "portable.h"
#include "json-parser.h"

static const char *ep;

#ifndef JSON_malloc
#define JSON_malloc malloc
#endif

#ifndef JSON_free
#define JSON_free free
#endif

#ifndef JSON_strchr
#define JSON_strchr strchr
#endif

const char *
NC_P(json_get_error_ptr)(void)
{
    return ep;
}

static int
NC_P(JSON_strcasecmp)(const char *s1, const char *s2)
{
    if (!s1) {
        return (s1 == s2) ? 0 : 1;
    }

    if (!s2) {
        return 1;
    }

    for (; tolower(*s1) == tolower(*s2); ++s1, ++s2) if (*s1 == 0) {
            return 0;
        }

    return tolower(*(const unsigned char *)s1) - tolower(*(const unsigned char *)s2);
}

static char *
NC_P(JSON_strdup)(const char *str)
{
    size_t len;
    char *copy;

    len = strlen(str) + 1;

    if (!(copy = (char *)JSON_malloc(len))) {
        return 0;
    }

    memcpy(copy, str, len);
    return copy;
}

/* Internal constructor. */
static JSON *
NC_P(JSON_New_Item)(void)
{
    JSON *node = (JSON *)JSON_malloc(sizeof(JSON));

    if (node) {
        memset(node, 0, sizeof(JSON));
    }

    return node;
}

/* Delete a JSON structure. */
void
NC_P(json_delete)(JSON *c)
{
    JSON *next;

    while (c) {
        next = c->next;

        if (!(c->type & JSON_IsReference) && c->child) {
            json_delete(c->child);
        }

        if (!(c->type & JSON_IsReference) && c->valuestring) {
            JSON_free(c->valuestring);
        }

        if (c->string) {
            JSON_free(c->string);
        }

        JSON_free(c);
        c = next;
    }
}

/* Parse the input text to generate a number, and populate the result into item. */
static const char *
NC_P(parse_number)(JSON *item, const char *num)
{
    double n = 0, sign = 1, scale = 0;
    int subscale = 0, signsubscale = 1;

    if (*num == '-') {
        sign = -1, num++;    /* Has sign? */
    }

    if (*num == '0') {
        num++;    /* is zero */
    }

    if (*num >= '1' && *num <= '9') do {
            n = (n * 10.0) + (*num++ -'0');
        }   while (*num >= '0' && *num <= '9'); /* Number? */

    if (*num == '.' && num[1] >= '0' && num[1] <= '9') {
        num++;           /* Fractional part? */

        do {
            n = (n * 10.0) + (*num++ -'0'), scale--;
        } while (*num >= '0' && *num <= '9');
    }

    if (*num == 'e' || *num == 'E') { /* Exponent? */
        num++;

        if (*num == '+') {
            num++;
        } else if (*num == '-') {
            signsubscale = -1, num++;    /* With sign? */
        }

        while (*num >= '0' && *num <= '9') {
            subscale = (subscale * 10) + (*num++ - '0');    /* Number? */
        }
    }

    n = sign * n * pow(10.0, (scale + subscale * signsubscale)); /* number = +/- number.fraction * 10^+/- exponent */

    item->valuedouble = n;
    item->valueint = (int)n;
    item->type = JSON_Number;
    return num;
}

/* Render the number nicely from the given item into a string. */
static char *
NC_P(print_number)(JSON *item)
{
    char *str;
    double d = item->valuedouble;

    if (fabs(((double)item->valueint) - d) <= DBL_EPSILON && d <= INT_MAX && d >= INT_MIN) {
        str = (char *)JSON_malloc(21); /* 2^64+1 can be represented in 21 chars. */

        if (str) {
        	sprintf(str, "%d", item->valueint);
        }
    } else {
        str = (char *)JSON_malloc(64); /* This is a nice tradeoff. */

        if (str) {
            //if (fabs(floor(d) - d) <= DBL_EPSILON && fabs(d) < 1.0e60) {
            if (fabs(d) < 1.0e60) {
            	sprintf(str, "%.0f", d);
            } else if (fabs(d) < 1.0e-6 || fabs(d) > 1.0e9) {
                sprintf(str, "%e", d);
            } else {
            	sprintf(str, "%f", d);
            }
        }
    }

    return str;
}

static unsigned
NC_P(parse_hex4)(const char *str)
{
    unsigned h = 0;

    if (*str >= '0' && *str <= '9') {
        h += (*str) - '0';
    } else if (*str >= 'A' && *str <= 'F') {
        h += 10 + (*str) - 'A';
    } else if (*str >= 'a' && *str <= 'f') {
        h += 10 + (*str) - 'a';
    } else {
        return 0;
    }

    h = h << 4;
    str++;

    if (*str >= '0' && *str <= '9') {
        h += (*str) - '0';
    } else if (*str >= 'A' && *str <= 'F') {
        h += 10 + (*str) - 'A';
    } else if (*str >= 'a' && *str <= 'f') {
        h += 10 + (*str) - 'a';
    } else {
        return 0;
    }

    h = h << 4;
    str++;

    if (*str >= '0' && *str <= '9') {
        h += (*str) - '0';
    } else if (*str >= 'A' && *str <= 'F') {
        h += 10 + (*str) - 'A';
    } else if (*str >= 'a' && *str <= 'f') {
        h += 10 + (*str) - 'a';
    } else {
        return 0;
    }

    h = h << 4;
    str++;

    if (*str >= '0' && *str <= '9') {
        h += (*str) - '0';
    } else if (*str >= 'A' && *str <= 'F') {
        h += 10 + (*str) - 'A';
    } else if (*str >= 'a' && *str <= 'f') {
        h += 10 + (*str) - 'a';
    } else {
        return 0;
    }

    return h;
}

/* Parse the input text into an unescaped cstring, and populate item. */
static const unsigned char firstByteMark[7] = { 0x00, 0x00, 0xC0, 0xE0, 0xF0, 0xF8, 0xFC };
static const char *
NC_P(parse_string)(JSON *item, const char *str)
{
    const char *ptr = str + 1;
    char *ptr2;
    char *out;
    int len = 0;
    unsigned uc, uc2;

    if (*str != '\"') {
        ep = str;    /* not a string! */
        return 0;
    }

    while (*ptr != '\"' && *ptr && ++len) if (*ptr++ == '\\') {
            ptr++;    /* Skip escaped quotes. */
        }

    out = (char *)JSON_malloc(len + 1); /* This is how long we need for the string, roughly. */

    if (!out) {
        return 0;
    }

    ptr = str + 1;
    ptr2 = out;

    while (*ptr != '\"' && *ptr) {
        if (*ptr != '\\') {
            *ptr2++ = *ptr++;
        } else {
            ptr++;

            switch (*ptr) {
                case 'b':
                    *ptr2++ = '\b';
                    break;

                case 'f':
                    *ptr2++ = '\f';
                    break;

                case 'n':
                    *ptr2++ = '\n';
                    break;

                case 'r':
                    *ptr2++ = '\r';
                    break;

                case 't':
                    *ptr2++ = '\t';
                    break;

                case 'u':    /* transcode utf16 to utf8. */
                    uc = parse_hex4(ptr + 1);
                    ptr += 4;  /* get the unicode char. */

                    if ((uc >= 0xDC00 && uc <= 0xDFFF) || uc == 0) {
                        break;    /* check for invalid. */
                    }

                    if (uc >= 0xD800 && uc <= 0xDBFF) { /* UTF16 surrogate pairs.   */
                        if (ptr[1] != '\\' || ptr[2] != 'u') {
                            break;    /* missing second-half of surrogate.  */
                        }

                        uc2 = parse_hex4(ptr + 3);
                        ptr += 6;

                        if (uc2 < 0xDC00 || uc2 > 0xDFFF) {
                            break;    /* invalid second-half of surrogate.  */
                        }

                        uc = 0x10000 + (((uc & 0x3FF) << 10) | (uc2 & 0x3FF));
                    }

                    len = 4;

                    if (uc < 0x80) {
                        len = 1;
                    } else if (uc < 0x800) {
                        len = 2;
                    } else if (uc < 0x10000) {
                        len = 3;
                    }

                    ptr2 += len;

                    switch (len) {
                        case 4:
                            *--ptr2 = ((uc | 0x80) & 0xBF);
                            uc >>= 6;
                            /* fall through */
                        case 3:
                            *--ptr2 = ((uc | 0x80) & 0xBF);
                            uc >>= 6;
                            /* fall through */
                        case 2:
                            *--ptr2 = ((uc | 0x80) & 0xBF);
                            uc >>= 6;
                            /* fall through */
                        case 1:
                            *--ptr2 = (uc | firstByteMark[len]);
                    }

                    ptr2 += len;
                    break;

                default:
                    *ptr2++ = *ptr;
                    break;
            }

            ptr++;
        }
    }

    *ptr2 = 0;

    if (*ptr == '\"') {
        ptr++;
    }

    item->valuestring = out;
    item->type = JSON_String;
    return ptr;
}

/* Render the cstring provided to an escaped version that can be printed. */
static char *
NC_P(print_string_ptr)(const char *str)
{
    const char *ptr;
    char *ptr2, *out;
    int len = 0;
    unsigned char token;

    if (!str) {
        return JSON_strdup("");
    }

    ptr = str;

    while ((token = *ptr) && ++len) {
        if (JSON_strchr("\"\\\b\f\n\r\t", token)) {
            len++;
        } else if (token < 32) {
            len += 5;
        }

        ptr++;
    }

    out = (char *)JSON_malloc(len + 3);

    if (!out) {
        return 0;
    }

    ptr2 = out;
    ptr = str;
    *ptr2++ = '\"';

    while (*ptr) {
        if ((unsigned char)*ptr > 31 && *ptr != '\"' && *ptr != '\\') {
            *ptr2++ = *ptr++;
        } else {
            *ptr2++ = '\\';

            switch (token = *ptr++) {
                case '\\':
                    *ptr2++ = '\\';
                    break;

                case '\"':
                    *ptr2++ = '\"';
                    break;

                case '\b':
                    *ptr2++ = 'b';
                    break;

                case '\f':
                    *ptr2++ = 'f';
                    break;

                case '\n':
                    *ptr2++ = 'n';
                    break;

                case '\r':
                    *ptr2++ = 'r';
                    break;

                case '\t':
                    *ptr2++ = 't';
                    break;

                default:
                	sprintf(ptr2, "u%04x", token);
                    ptr2 += 5;
                    break;  /* escape and print */
            }
        }
    }

    *ptr2++ = '\"';
    *ptr2++ = 0;
    return out;
}
/* Invote print_string_ptr (which is useful) on an item. */
static char *
NC_P(print_string)(JSON *item)
{
    return print_string_ptr(item->valuestring);
}

/* Predeclare these prototypes. */
static const char *parse_value(JSON *item, const char *value);
static char *print_value(JSON *item, int depth, int fmt);
static const char *parse_array(JSON *item, const char *value);
static char *print_array(JSON *item, int depth, int fmt);
static const char *parse_object(JSON *item, const char *value);
static char *print_object(JSON *item, int depth, int fmt);

/* Utility to jump whitespace and cr/lf */
static const char *
NC_P(skip)(const char *in)
{
    while (in && *in && (unsigned char)*in <= 32) {
        in++;
    }

    return in;
}

/* Parse an object - create a new root, and populate. */
JSON *
NC_P(json_parse_with_opts)(const char *value, const char **return_parse_end, int require_null_terminated)
{
    const char *end = 0;
    JSON *c = JSON_New_Item();
    ep = 0;

    if (!c) {
        return 0;    /* memory fail */
    }

    end = parse_value(c, skip(value));

    if (!end)   {
        json_delete(c);    /* parse failure. ep is set. */
        return 0;
    }

    /* if we require null-terminated JSON without appended garbage, skip and then check for a null terminator */
    if (require_null_terminated) {
        end = skip(end);

        if (*end) {
            json_delete(c);
            ep = end;
            return 0;
        }
    }

    if (return_parse_end) {
        *return_parse_end = end;
    }

    return c;
}
/* Default options for JSON_Parse */
JSON *
NC_P(json_parse)(const char *value)
{
    return json_parse_with_opts(value, 0, 0);
}

/* Render a JSON item/entity/structure to text. */
char *
NC_P(json_print)(JSON *item)
{
    return print_value(item, 0, 1);
}
char *
NC_P(json_print_unformatted)(JSON *item)
{
    return print_value(item, 0, 0);
}

/* Parser core - when encountering text, process appropriately. */
static const char *
NC_P(parse_value)(JSON *item, const char *value)
{
    if (!value) {
        return 0;    /* Fail on null. */
    }

    if (!strncmp(value, "null", 4))    {
        item->type = JSON_NULL;
        return value + 4;
    }

    if (!strncmp(value, "false", 5))   {
        item->type = JSON_False;
        return value + 5;
    }

    if (!strncmp(value, "true", 4))    {
        item->type = JSON_True;
        item->valueint = 1;
        return value + 4;
    }

    if (*value == '\"')               {
        return parse_string(item, value);
    }

    if (*value == '-' || (*value >= '0' && *value <= '9'))    {
        return parse_number(item, value);
    }

    if (*value == '[')                {
        return parse_array(item, value);
    }

    if (*value == '{')                {
        return parse_object(item, value);
    }

    ep = value;
    return 0;  /* failure. */
}

/* Render a value to text. */
static char *
NC_P(print_value)(JSON *item, int depth, int fmt)
{
    char *out = 0;

    if (!item) {
        return 0;
    }

    switch ((item->type) & 255) {
        case JSON_NULL:
            out = JSON_strdup("null");
            break;

        case JSON_False:
            out = JSON_strdup("false");
            break;

        case JSON_True:
            out = JSON_strdup("true");
            break;

        case JSON_Number:
            out = print_number(item);
            break;

        case JSON_String:
            out = print_string(item);
            break;

        case JSON_Array:
            out = print_array(item, depth, fmt);
            break;

        case JSON_Object:
            out = print_object(item, depth, fmt);
            break;
    }

    return out;
}

/* Build an array from input text. */
static const char *
NC_P(parse_array)(JSON *item, const char *value)
{
    JSON *child;

    if (*value != '[')    {
        ep = value;    /* not an array! */
        return 0;
    }

    item->type = JSON_Array;
    value = skip(value + 1);

    if (*value == ']') {
        return value + 1;    /* empty array. */
    }

    item->child = child = JSON_New_Item();

    if (!item->child) {
        return 0;    /* memory fail */
    }

    value = skip(parse_value(child, skip(value))); /* skip any spacing, get the value. */

    if (!value) {
        return 0;
    }

    while (*value == ',') {
        JSON *new_item;

        if (!(new_item = JSON_New_Item())) {
            return 0;    /* memory fail */
        }

        child->next = new_item;
        new_item->prev = child;
        child = new_item;
        value = skip(parse_value(child, skip(value + 1)));

        if (!value) {
            return 0;    /* memory fail */
        }
    }

    if (*value == ']') {
        return value + 1;    /* end of array */
    }

    ep = value;
    return 0;  /* malformed. */
}

/* Render an array to text */
static char *
NC_P(print_array)(JSON *item, int depth, int fmt)
{
    char **entries;
    char *out = 0, *ptr, *ret;
    int len = 5;
    JSON *child = item->child;
    int numentries = 0, i = 0, fail = 0;

    /* How many entries in the array? */
    while (child) {
        numentries++, child = child->next;
    }

    /* Explicitly handle numentries==0 */
    if (!numentries) {
        out = (char *)JSON_malloc(3);

        if (out) {
            strcpy(out, "[]");
        }

        return out;
    }

    /* Allocate an array to hold the values for each */
    entries = (char **)JSON_malloc(numentries * sizeof(char *));

    if (!entries) {
        return 0;
    }

    memset(entries, 0, numentries * sizeof(char *));
    /* Retrieve all the results: */
    child = item->child;

    while (child && !fail) {
        ret = print_value(child, depth + 1, fmt);
        entries[i++] = ret;

        if (ret) {
            len += strlen(ret) + 2 + (fmt ? 1 : 0);
        } else {
            fail = 1;
        }

        child = child->next;
    }

    /* If we didn't fail, try to malloc the output string */
    if (!fail) {
        out = (char *)JSON_malloc(len);
    }

    /* If that fails, we fail. */
    if (!out) {
        fail = 1;
    }

    /* Handle failure. */
    if (fail) {
        for (i = 0; i < numentries; i++) if (entries[i]) {
                JSON_free(entries[i]);
            }

        JSON_free(entries);
        return 0;
    }

    /* Compose the output array. */
    *out = '[';
    ptr = out + 1;
    *ptr = 0;

    for (i = 0; i < numentries; i++) {
        strcpy(ptr, entries[i]);
        ptr += strlen(entries[i]);

        if (i != numentries - 1) {
            *ptr++ = ',';

            if (fmt) {
                *ptr++ = ' ';
            }*ptr = 0;
        }

        JSON_free(entries[i]);
    }

    JSON_free(entries);
    *ptr++ = ']';
    *ptr++ = 0;
    return out;
}

/* Build an object from the text. */
static const char *
NC_P(parse_object)(JSON *item, const char *value)
{
    JSON *child;

    if (*value != '{')    {
        ep = value;    /* not an object! */
        return 0;
    }

    item->type = JSON_Object;
    value = skip(value + 1);

    if (*value == '}') {
        return value + 1;    /* empty array. */
    }

    item->child = child = JSON_New_Item();

    if (!item->child) {
        return 0;
    }

    value = skip(parse_string(child, skip(value)));

    if (!value) {
        return 0;
    }

    child->string = child->valuestring;
    child->valuestring = 0;

    if (*value != ':') {
        ep = value;    /* fail! */
        return 0;
    }

    value = skip(parse_value(child, skip(value + 1))); /* skip any spacing, get the value. */

    if (!value) {
        return 0;
    }

    while (*value == ',') {
        JSON *new_item;

        if (!(new_item = JSON_New_Item())) {
            return 0;    /* memory fail */
        }

        child->next = new_item;
        new_item->prev = child;
        child = new_item;
        value = skip(parse_string(child, skip(value + 1)));

        if (!value) {
            return 0;
        }

        child->string = child->valuestring;
        child->valuestring = 0;

        if (*value != ':') {
            ep = value;    /* fail! */
            return 0;
        }

        value = skip(parse_value(child, skip(value + 1))); /* skip any spacing, get the value. */

        if (!value) {
            return 0;
        }
    }

    if (*value == '}') {
        return value + 1;    /* end of array */
    }

    ep = value;
    return 0;  /* malformed. */
}

/* Render an object to text. */
static char *
NC_P(print_object)(JSON *item, int depth, int fmt)
{
    char **entries = 0, **names = 0;
    char *out = 0, *ptr, *ret, *str;
    int len = 7, i = 0, j;
    JSON *child = item->child;
    int numentries = 0, fail = 0;

    /* Count the number of entries. */
    while (child) {
        numentries++, child = child->next;
    }

    /* Explicitly handle empty object case */
    if (!numentries) {
        out = (char *)JSON_malloc(fmt ? depth + 4 : 3);

        if (!out) {
            return 0;
        }

        ptr = out;
        *ptr++ = '{';

        if (fmt) {
            *ptr++ = '\n';

            for (i = 0; i < depth - 1; i++) {
                *ptr++ = '\t';
            }
        }

        *ptr++ = '}';
        *ptr++ = 0;
        return out;
    }

    /* Allocate space for the names and the objects */
    entries = (char **)JSON_malloc(numentries * sizeof(char *));

    if (!entries) {
        return 0;
    }

    names = (char **)JSON_malloc(numentries * sizeof(char *));

    if (!names) {
        JSON_free(entries);
        return 0;
    }

    memset(entries, 0, sizeof(char *)*numentries);
    memset(names, 0, sizeof(char *)*numentries);

    /* Collect all the results into our arrays: */
    child = item->child;
    depth++;

    if (fmt) {
        len += depth;
    }

    while (child) {
        names[i] = str = print_string_ptr(child->string);
        entries[i++] = ret = print_value(child, depth, fmt);

        if (str && ret) {
            len += strlen(ret) + strlen(str) + 2 + (fmt ? 2 + depth : 0);
        } else {
            fail = 1;
        }

        child = child->next;
    }

    /* Try to allocate the output string */
    if (!fail) {
        out = (char *)JSON_malloc(len);
    }

    if (!out) {
        fail = 1;
    }

    /* Handle failure */
    if (fail) {
        for (i = 0; i < numentries; i++) {
            if (names[i]) {
                JSON_free(names[i]);
            }

            if (entries[i]) {
                JSON_free(entries[i]);
            }
        }

        JSON_free(names);
        JSON_free(entries);
        return 0;
    }

    /* Compose the output: */
    *out = '{';
    ptr = out + 1;

    if (fmt) {
        *ptr++ = '\n';
    }*ptr = 0;

    for (i = 0; i < numentries; i++) {
        if (fmt) for (j = 0; j < depth; j++) {
                *ptr++ = '\t';
            }

        strcpy(ptr, names[i]);
        ptr += strlen(names[i]);
        *ptr++ = ':';

        if (fmt) {
            *ptr++ = '\t';
        }

        strcpy(ptr, entries[i]);
        ptr += strlen(entries[i]);

        if (i != numentries - 1) {
            *ptr++ = ',';
        }

        if (fmt) {
            *ptr++ = '\n';
        }*ptr = 0;

        JSON_free(names[i]);
        JSON_free(entries[i]);
    }

    JSON_free(names);
    JSON_free(entries);

    if (fmt) for (i = 0; i < depth - 1; i++) {
            *ptr++ = '\t';
        }

    *ptr++ = '}';
    *ptr++ = 0;
    return out;
}

/* Get Array size/item / object item. */
int
NC_P(json_get_array_size)(JSON *array)
{
    JSON *c = array->child;
    int i = 0;

    while (c) {
        i++, c = c->next;
    }

    return i;
}
JSON *
NC_P(json_get_array_item)(JSON *array, int item)
{
    JSON *c = array->child;

    while (c && item > 0) {
        item--, c = c->next;
    }

    return c;
}
JSON *
NC_P(json_get_object_item)(JSON *object, const char *string)
{
    JSON *c = object->child;

    while (c && JSON_strcasecmp(c->string, string)) {
        c = c->next;
    }

    return c;
}

/* Utility for array list handling. */
static void
NC_P(suffix_object)(JSON *prev, JSON *item)
{
    prev->next = item;
    item->prev = prev;
}
/* Utility for handling references. */
static JSON *
NC_P(create_reference)(JSON *item)
{
    JSON *ref = JSON_New_Item();

    if (!ref) {
        return 0;
    }

    memcpy(ref, item, sizeof(JSON));
    ref->string = 0;
    ref->type |= JSON_IsReference;
    ref->next = ref->prev = 0;
    return ref;
}

/* Add item to array/object. */
void
NC_P(json_add_item_to_array)(JSON *array, JSON *item)
{
    JSON *c = array->child;

    if (!item) {
        return;
    }

    if (!c) {
        array->child = item;
    } else {
        while (c && c->next) {
            c = c->next;
        }

        suffix_object(c, item);
    }
}
void
NC_P(json_add_item_to_object)(JSON *object, const char *string, JSON *item)
{
    if (!item) {
        return;
    }

    if (item->string) {
        JSON_free(item->string);
    }

    item->string = JSON_strdup(string);
    json_add_item_to_array(object, item);
}
void
NC_P(json_add_item_reference_to_array)(JSON *array, JSON *item)
{
    json_add_item_to_array(array, create_reference(item));
}
void
NC_P(jspn_add_item_reference_to_object)(JSON *object, const char *string, JSON *item)
{
    json_add_item_to_object(object, string, create_reference(item));
}

JSON *
NC_P(json_detach_item_from_array)(JSON *array, int which)
{
    JSON *c = array->child;

    while (c && which > 0) {
        c = c->next, which--;
    }

    if (!c) {
        return 0;
    }

    if (c->prev) {
        c->prev->next = c->next;
    }

    if (c->next) {
        c->next->prev = c->prev;
    }

    if (c == array->child) {
        array->child = c->next;
    }

    c->prev = c->next = 0;
    return c;
}
void
NC_P(json_delete_item_from_array)(JSON *array, int which)
{
    json_delete(json_detach_item_from_array(array, which));
}
JSON *
NC_P(json_detach_item_from_object)(JSON *object, const char *string)
{
    int i = 0;
    JSON *c = object->child;

    while (c && JSON_strcasecmp(c->string, string)) {
        i++, c = c->next;
    }

    if (c) {
        return json_detach_item_from_array(object, i);
    }

    return 0;
}
void
NC_P(json_delete_item_from_object)(JSON *object, const char *string)
{
    json_delete(json_detach_item_from_object(object, string));
}

/* Replace array/object items with new ones. */
void
NC_P(json_replace_item_inarray)(JSON *array, int which, JSON *newitem)
{
    JSON *c = array->child;

    while (c && which > 0) {
        c = c->next, which--;
    }

    if (!c) {
        return;
    }

    newitem->next = c->next;
    newitem->prev = c->prev;

    if (newitem->next) {
        newitem->next->prev = newitem;
    }

    if (c == array->child) {
        array->child = newitem;
    } else {
        newitem->prev->next = newitem;
    }

    c->next = c->prev = 0;
    json_delete(c);
}
void
NC_P(json_replace_item_inobject)(JSON *object, const char *string, JSON *newitem)
{
    int i = 0;
    JSON *c = object->child;

    while (c && JSON_strcasecmp(c->string, string)) {
        i++, c = c->next;
    }

    if (c) {
        newitem->string = JSON_strdup(string);
        json_replace_item_inarray(object, i, newitem);
    }
}

/* Create basic types: */
JSON *
NC_P(json_create_null)(void)
{
    JSON *item = JSON_New_Item();

    if (item) {
        item->type = JSON_NULL;
    }

    return item;
}
JSON *
NC_P(json_create_true)(void)
{
    JSON *item = JSON_New_Item();

    if (item) {
        item->type = JSON_True;
    }

    return item;
}
JSON *
NC_P(json_create_false)(void)
{
    JSON *item = JSON_New_Item();

    if (item) {
        item->type = JSON_False;
    }

    return item;
}
JSON *
NC_P(json_create_bool)(int b)
{
    JSON *item = JSON_New_Item();

    if (item) {
        item->type = b ? JSON_True : JSON_False;
    }

    return item;
}
JSON *
NC_P(json_create_number)(double num)
{
    JSON *item = JSON_New_Item();

    if (item) {
        item->type = JSON_Number;
        item->valuedouble = num;
        item->valueint = (int)num;
    }

    return item;
}
JSON *
NC_P(json_create_string)(const char *string)
{
    JSON *item = JSON_New_Item();

    if (item) {
        item->type = JSON_String;
        item->valuestring = JSON_strdup(string);
    }

    return item;
}
JSON *
NC_P(json_create_array)(void)
{
    JSON *item = JSON_New_Item();

    if (item) {
        item->type = JSON_Array;
    }

    return item;
}
JSON *
NC_P(json_create_object)(void)
{
    JSON *item = JSON_New_Item();

    if (item) {
        item->type = JSON_Object;
    }

    return item;
}

/* Create Arrays: */
JSON *
NC_P(json_create_int_array)(const int *numbers, int count)
{
    int i;
    JSON *n = 0, *p = 0, *a = json_create_array();

    for (i = 0; a && i < count; i++) {
        n = json_create_number(numbers[i]);

        if (!i) {
            a->child = n;
        } else {
            suffix_object(p, n);
        }

        p = n;
    }

    return a;
}
JSON *
NC_P(json_create_float_array)(const float *numbers, int count)
{
    int i;
    JSON *n = 0, *p = 0, *a = json_create_array();

    for (i = 0; a && i < count; i++) {
        n = json_create_number(numbers[i]);

        if (!i) {
            a->child = n;
        } else {
            suffix_object(p, n);
        }

        p = n;
    }

    return a;
}
JSON *
NC_P(json_create_double_array)(const double *numbers, int count)
{
    int i;
    JSON *n = 0, *p = 0, *a = json_create_array();

    for (i = 0; a && i < count; i++) {
        n = json_create_number(numbers[i]);

        if (!i) {
            a->child = n;
        } else {
            suffix_object(p, n);
        }

        p = n;
    }

    return a;
}
JSON *
NC_P(json_create_string_array)(const char **strings, int count)
{
    int i;
    JSON *n = 0, *p = 0, *a = json_create_array();

    for (i = 0; a && i < count; i++) {
        n = json_create_string(strings[i]);

        if (!i) {
            a->child = n;
        } else {
            suffix_object(p, n);
        }

        p = n;
    }

    return a;
}

/* Duplication */
JSON *
NC_P(json_duplicate)(JSON *item, int recurse)
{
    JSON *newitem, *cptr, *nptr = 0, *newchild;

    /* Bail on bad ptr */
    if (!item) {
        return 0;
    }

    /* Create new item */
    newitem = JSON_New_Item();

    if (!newitem) {
        return 0;
    }

    /* Copy over all vars */
    newitem->type = item->type & (~JSON_IsReference), newitem->valueint = item->valueint, newitem->valuedouble = item->valuedouble;

    if (item->valuestring)  {
        newitem->valuestring = JSON_strdup(item->valuestring);

        if (!newitem->valuestring)  {
            json_delete(newitem);
            return 0;
        }
    }

    if (item->string)       {
        newitem->string = JSON_strdup(item->string);

        if (!newitem->string)       {
            json_delete(newitem);
            return 0;
        }
    }

    /* If non-recursive, then we're done! */
    if (!recurse) {
        return newitem;
    }

    /* Walk the ->next chain for the child. */
    cptr = item->child;

    while (cptr) {
        newchild = json_duplicate(cptr, 1);    /* Duplicate (with recurse) each item in the ->next chain */

        if (!newchild) {
            json_delete(newitem);
            return 0;
        }

        if (nptr)   {
            nptr->next = newchild, newchild->prev = nptr;    /* If newitem->child already set, then crosswire ->prev and ->next and move on */
            nptr = newchild;
        } else        {
            newitem->child = newchild;    /* Set newitem->child and move to it */
            nptr = newchild;
        }

        cptr = cptr->next;
    }

    return newitem;
}

void
NC_P(json_minify)(char *json)
{
    char *into = json;

    while (*json) {
        if (*json == ' ') {
            json++;
        } else if (*json == '\t') {
            json++;    // Whitespace characters.
        } else if (*json == '\r') {
            json++;
        } else if (*json == '\n') {
            json++;
        } else if (*json == '/' && json[1] == '/')  while (*json && *json != '\n') {
                json++;    // double-slash comments, to end of line.
            }
        else if (*json == '/' && json[1] == '*') {
            while (*json && !(*json == '*' && json[1] == '/')) {
                json++;    // multiline comments.
            }

            json += 2;
        } else if (*json == '\"') {
            *into++ = *json++;    // string literals, which are \" sensitive.

            while (*json && *json != '\"') {
                if (*json == '\\') {
                    *into++ = *json++;
                }*into++ = *json++;
            }*into++ = *json++;
        } else {
            *into++ = *json++;    // All other characters.
        }
    }

    *into = 0;  // and null-terminate.
}

