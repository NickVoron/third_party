#include <stdio.h>
#include "libpsd.h"
#include "psd_stream.h"
#include "utfcvt/library.include.h"

#define ASCII_LF 012
#define ASCII_CR 015

#define MAX_NAMES 32
#define MAX_DICTS 32 // dict/array nesting limit

int is_pdf_white(char c){
	return c == '\000' || c == '\011' || c == '\012'
		|| c == '\014' || c == '\015' || c == '\040';
}

int is_pdf_delim(char c){
	return c == '(' || c == ')' || c == '<' || c == '>'
		|| c == '[' || c == ']' || c == '{' || c == '}'
		|| c == '/' || c == '%';
}

int hexdigit(unsigned char c) {
	c = toupper(c);
	return c - (c >= 'A' ? 'A' - 10 : '0');
}

const char *tabs(int n) {
	static const char forty [] = "\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t\t";
	return forty + (sizeof(forty) - 1 - n);
}

int xmlcursor = 0;
char* xmlfile = 0;

// p      : pointer to first character following opening ( of string
// outbuf : destination buffer for parsed string. pass NULL to count but not store
// n      : count of characters available in input buffer
// returns number of characters in parsed string
// updates the source pointer to the first character after the string

size_t pdf_string(char **p, char *outbuf, size_t n){
	int paren = 1;
	size_t cnt;
	char c;

	for(cnt = 0; n;){
		--n;
		switch(c = *(*p)++){
		case ASCII_CR:
			if(n && (*p)[0] == ASCII_LF){ // eat the linefeed in a cr/lf pair
				++(*p);
				--n;
			}
			c = ASCII_LF;
		case ASCII_LF:
			break;
		case '(':
			++paren;
			break;
		case ')':
			if(!(--paren))
				return cnt; // it was the closing paren
			break;
		case '\\':
			if(!n)
				return cnt; // ran out of data
			--n;
			switch(c = *(*p)++){
			case ASCII_CR: // line continuation; skip newline
				if(n && (*p)[0] == ASCII_LF){ // eat the linefeed in a cr/lf pair
					++(*p);
					--n;
				}
			case ASCII_LF:
				continue;
			case 'n': c = ASCII_LF; break;
			case 'r': c = ASCII_CR; break;
			case 't': c = 011; break; // horizontal tab
			case 'b': c = 010; break; // backspace
			case 'f': c = 014; break; // formfeed
			case '(':
			case ')':
			case '\\': c = (*p)[-1]; break;
			case '0': case '1': case '2': case '3':
			case '4': case '5': case '6': case '7':
				// octal escape
				c = (*p)[-1] - '0';
				if(n >= 1 && isdigit((*p)[0])){
					c = (c << 3) | ((*p)[0] - '0');
					++p;
					--n;
					if(n >= 1 && isdigit((*p)[0])){
						c = (c << 3) | ((*p)[0] - '0');
						++p;
						--n;
					}
				}
				break;
			}
		}
		if(outbuf)
			*outbuf++ = c;
		++cnt;
	}
	return cnt;
}

// parameters analogous to pdf_string()'s
size_t pdf_hexstring(char **p, char *outbuf, size_t n)
{
	size_t cnt, flag;
	unsigned acc;

	for(cnt = acc = flag = 0; n;){
		char c = *(*p)++;
		--n;
		if(c == '>'){ // or should this be pdf_delim() ?
			// check for partial byte
			if(flag){
				if(outbuf)
					*outbuf++ = acc;
				++cnt;
			}
			break;
		}else if(!is_pdf_white(c)){ // N.B. DOES NOT CHECK for valid hex digits!
			acc |= hexdigit(c);
			if(flag){
				// both nibbles loaded; emit character
				if(outbuf)
					*outbuf++ = acc;
				++cnt;
				flag = acc = 0;
			}else{
				acc <<= 4;
				flag = 1; // high nibble loaded
			}
		}
	}
	return cnt;
}

// parameters analogous to pdf_string()'s
size_t pdf_name(char **p, char *outbuf, size_t n){
	size_t cnt;

	for(cnt = 0; n;){
		char c = *(*p);
		if(is_pdf_white(c) || is_pdf_delim(c))
			break;
		else if(c == '#' && n >= 3){
			// process #XX hex escape
			c = (hexdigit((*p)[1]) << 4) | hexdigit((*p)[2]);
			*p += 3;
			n -= 3;
		}else{
			// consume character
			++(*p);
			--n;
		}
		if(outbuf)
			*outbuf++ = c;
		++cnt;
	}
	return cnt;
}

static char *name_stack[MAX_NAMES];
static unsigned name_tos, in_array;

void push_name(char *tag) {
	if (name_tos == MAX_NAMES)
		printf("name stack overflow");
	name_stack[name_tos++] = tag;
}

void pop_name() {
	if (name_tos)
		free(name_stack[--name_tos]);
	else
		printf("pop_name(): underflow");
}

// Write a string representation to XML. Either convert to UTF-8
// from the UTF-16BE if flagged as such by a BOM prefix,
// or just write the literal string bytes without transliteration.

/*
From PDF Reference 1.7, 3.8.1 Text String Type

The text string type is used for character strings that are encoded in either PDFDocEncoding
or the UTF-16BE Unicode character encoding scheme. PDFDocEncoding
can encode all of the ISO Latin 1 character set and is documented in Appendix D. ...

For text strings encoded in Unicode, the first two bytes must be 254 followed by 255.
These two bytes represent the Unicode byte order marker, U+FEFF,
indicating that the string is encoded in the UTF-16BE (big-endian) encoding scheme ...

Note: Applications that process PDF files containing Unicode text strings
should be prepared to handle supplementary characters;
that is, characters requiring more than two bytes to represent.
*/

char* decode(char *strbuf, size_t cnt)
{
	if (cnt >= 2)
	{
		if (strbuf[0] == (char)0xfe && strbuf[1] == (char)0xff)
		{
			return &strbuf[2];
		}
	}

	return "error";
}

void UTF_BE2LE(UTF16* strbuf, size_t cnt)
{
	for (size_t i = 0; i < cnt; ++i)
	{
		UTF16 v = strbuf[i];
		unsigned char* cv = (unsigned char*) &v;

		char v0 = cv[0];
		char v1 = cv[1];
		cv[0] = v1;
		cv[1] = v0;

		strbuf[i] = v;
	}
}

void stringxml(char *strbuf, size_t cnt)
{
	size_t count = ((cnt) / 2) - 1;

	UTF16 buf16[1024];
	UTF8 buffer[1024];

	memcpy(&buf16, (UTF16*)&strbuf[2], sizeof(UTF16) * count);

	UTF_BE2LE(&buf16[0], count);
	buf16[count] = 0;

	UTF8* trgS = &buffer[0];
	UTF16* srcS = &buf16[0];

 	ConversionResult res = ConvertUTF16toUTF8(&srcS, &buf16[count + 1], &trgS, &buffer[1024], 0);
	xmlcursor += sprintf(&xmlfile[xmlcursor], buffer);
	xmlfile[xmlcursor] = 0;
}

void begin_element(const char *indent)
{
	if(in_array)
		xmlcursor += sprintf(&xmlfile[xmlcursor], "%s<e>", indent);
	else if(name_tos)
		xmlcursor += sprintf(&xmlfile[xmlcursor], "%s<%s>", indent, name_stack[name_tos-1]);

	xmlfile[xmlcursor] = 0;
}

void end_element(const char *indent)
{
	if(in_array)
	{
		xmlcursor += sprintf(&xmlfile[xmlcursor], "%s</e>\n", indent);
	}
	else if(name_tos){
		xmlcursor += sprintf(&xmlfile[xmlcursor], "%s</%s>\n", indent, name_stack[name_tos-1]);
		pop_name();
	}

	xmlfile[xmlcursor] = 0;
}

// Implements a "ghetto" PDF syntax parser - just the minimum needed
// to translate Photoshop's embedded type tool data into XML.

// PostScript implements a single heterogenous stack; we don't try
// to emulate proper behaviour here but rather keep a 'stack' of names
// only in order to generate correct closing tags,
// and remember whether the 'current' object is a dictionary or array.

static void pdf_data(char *buf, size_t n, int level)
{
	char *p, *q, *strbuf, c;
	size_t cnt;
	unsigned is_array[MAX_DICTS], dict_tos = 0;

	name_tos = in_array = 0;
	for(p = buf; n;){
		c = *p++;
		--n;
		switch(c){
		case '(':
			// String literal. Copy the string content to XML
			// as element content.

			// check parsed string length
			q = p;
			cnt = pdf_string(&q, NULL, n);

			// parse string into new buffer, and step past in source buffer
			strbuf = malloc(cnt+1);
			q = p;
			pdf_string(&p, strbuf, n);
			n -= p - q;
			strbuf[cnt] = 0;

			begin_element(tabs(level));
			stringxml(strbuf, cnt);
			end_element("");

			free(strbuf);
			break;

		case '<':
			if(n && *p == '<'){ // dictionary literal
				++p;
				--n;
		case '[':
				begin_element(tabs(level));
				if(name_tos){
					xmlcursor += sprintf(&xmlfile[xmlcursor], "\n");
					xmlfile[xmlcursor] = 0;
					++level;
				}

				if(dict_tos == MAX_DICTS)
					printf("dict stack overflow");
				is_array[dict_tos++] = in_array = c == '[';
			}
			else{ // hex string literal
				q = p;
				cnt = pdf_hexstring(&q, NULL, n);

				strbuf = malloc(cnt+1);
				q = p;
				pdf_hexstring(&p, strbuf, n);
				n -= p - q;
				strbuf[cnt] = 0;

				begin_element(tabs(level));
				stringxml(strbuf, cnt);
				end_element("");

				free(strbuf);
			}
			break;

		case '>':
			if(n && *p == '>'){
				++p;
				--n;
			}
			else{
				printf("misplaced >");
				continue;
			}
		case ']':
			if(dict_tos)
				--dict_tos;
			else
				printf("dict stack underflow");
			in_array = dict_tos && is_array[dict_tos-1];

			end_element(tabs(--level));
			break;

		case '/':
			// check parsed name length
			q = p;
			cnt = pdf_name(&q, NULL, n);

			// parse name into new buffer, and step past in source buffer
			strbuf = malloc(cnt+1);
			q = p;
			pdf_name(&p, strbuf, n);
			strbuf[cnt] = 0;
			n -= p - q;

			// FIXME: This won't work correctly if a name is given as a
			//        value for a dictionary key (we need to track of
			//        whether name is key or value)
			if(in_array){
				begin_element(tabs(level));
				stringxml(strbuf, cnt);
				end_element("");
				free(strbuf);
			}
			else{ // it's a dictionary key
				// FIXME: we need to deal with zero-length key, and
				//        characters unsuitable for XML element name.
				push_name(strbuf);
			}
			break;

		case '%': // skip comment
			while(n && *p != ASCII_CR && *p != ASCII_LF){
				++p;
				--n;
			}
			break;

		default:
			if(!is_pdf_white(c)){
				// numeric or boolean literal, or null
				// use characters until whitespace or delimiter
				q = p-1;
				while(n && !is_pdf_white(*p) && !is_pdf_delim(*p)){
					++p;
					--n;
				}

				c = *p;
				*p = 0;
				// If a dictionary value has value null, the key
				// should not be created. (7.3.7)
				if(in_array || strcmp(q, "null")){
					begin_element(tabs(level));
					xmlcursor += sprintf(&xmlfile[xmlcursor], q);
					xmlfile[xmlcursor] = 0;
					//fputs(q, );
					end_element("");
				}
				*p = c;
			}
			break;
		}
	}

	// close any open elements (should not happen)
	while(name_tos){
		printf("unclosed element %s", name_stack[name_tos-1]);
		pop_name();
	}
}

void desc_pdf(psd_context* context, psd_layer_record* layer)
{
	psd_int count = psd_stream_get_int(context);
	char *buf = malloc(count);

	if (buf)
	{
		xmlfile = (char*) malloc(1024 * 1024);
		count = psd_stream_get(context, buf, count);
		pdf_data(buf, count, 0);
		

		layer->pdf_xml_text = (char*) malloc(xmlcursor + 1);
		memcpy(layer->pdf_xml_text, xmlfile, xmlcursor);
		layer->pdf_xml_text[xmlcursor] = 0;

 		{
 			const char* fileName = "test.xml";
 			FILE* xfile = fopen(fileName, "w");
 			fwrite(layer->pdf_xml_text, xmlcursor, 1, xfile);
 			fclose(xfile);
 		}					

		
		free(buf);
		free(xmlfile);

		xmlfile = 0;
		xmlcursor = 0;
	}
}
