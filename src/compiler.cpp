
#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <unordered_map>

static const char HEX[] = "0123456789abcdef";
static uint64_t cursor_prog = 0x4000000000000000;
static uint64_t cursor_glob = 0x8000000000000000;
static const char* line_c;

static std::unordered_map<std::string, uint64_t> labels;
static std::string line;
static std::ofstream* out;

static std::string get_next_word(const char*& str)
{
	for(;;)
	{
		char c = str[0];

		if(c == '\0') {
			break;
		}

		if(c != ' ' && c != '\t') {
			break;
		}

		str += 1;
	}
		
	size_t len = 0;

	for(;;)
	{
		char c = str[len];

		if(c == ' ' || c == '\t' || c == '\n' || c == '\0') {
			break;
		}

		len += 1;
	}

	char* word = new char[len];

	for(size_t i = 0; i < len; i++) {
		word[i] = str[i];
	}

	str += len;
	std::string word_s(word, len);

	delete[] word;
	return word_s;
}

static void to_lower(std::string& str)
{
	for(size_t i = 0; i < str.length(); i++)
	{
		char c = str[i];

		if(c >= 'A' && c <= 'Z') {
			str[i] = c + 32;
		}
	}
}

static uint8_t decode_hex_char(char c)
{
	if(c >= '0' && c <= '9') {
		return (uint8_t)(c - '0');
	}

	if(c >= 'a' && c <= 'f') {
		return (uint8_t)(c - 'a' + 10);
	}

	if(c >= 'A' && c <= 'F') {
		return (uint8_t)(c - 'A' + 10);
	}

	return 0;
}

static std::string to_hex(uint64_t val)
{
	char data[16];

	for(int i = 15; i >= 0; i--)
	{
		data[i] = HEX[val & 15];
		val >>= 4;
	}

	return "0x" + std::string(data, 16);
}

static void put_uint8(uint8_t var)
{
	out->put((char)var);
}

static void put_uint16(uint16_t var)
{
	uint8_t data[2];
	data[0] = var >> 8;
	data[1] = var;

	out->write((char*)data, 2);
}

static void put_uint32(uint32_t var)
{
	uint8_t data[4];
	data[0] = var >> 24;
	data[1] = var >> 16;
	data[2] = var >> 8;
	data[3] = var;

	out->write((char*)data, 4);
}

static void put_uint64(uint64_t var)
{
	uint8_t data[8];
	data[0] = var >> 56;
	data[1] = var >> 48;
	data[2] = var >> 40;
	data[3] = var >> 32;
	data[4] = var >> 24;
	data[5] = var >> 16;
	data[6] = var >> 8;
	data[7] = var;

	out->write((char*)data, 8);
}

static uint64_t get_uint(std::string str)
{
	// hex
	if(str[0] == '0' && str[1] == 'x')
	{
		uint64_t val = 0;
		uint64_t mul = 1;

		for(uint64_t i = str.length() - 1; i > 1; i--)
		{
			val += decode_hex_char(str[i]) * mul;
			mul *= 16;
		}

		return val;
	}

	// num
	else
	{
		try {
			return std::stoul(str);
		}

		catch(std::invalid_argument& e) {}
		catch(std::out_of_range& e) {}

		auto item = labels.find(str);

		if(item == labels.end()) {
			throw "label or int \"" + str + "\" is invalid";
		} else {
			return item->second;
		}
	}
}

static std::string get_data(std::string str)
{
	// hex
	if(str[0] == '0' && str[1] == 'x')
	{
		std::stringstream val;
		uint64_t str_l = str.length();

		for(uint64_t i = 2; i < str_l; i += 2)
		{
			uint8_t v1 = decode_hex_char(str[i]) * 16;
			uint8_t v2;

			if(i + 1 < str_l) {
				v2 = decode_hex_char(str[i+1]);
			} else {
				v2 = 0;
			}

			val.put(v1 + v2);
		}

		return val.str();
	}

	// num
	else
	{
		throw "bad data";
	}
}

struct SIOP
{
	std::string name;
	uint8_t cmd;
	uint8_t ops;

	SIOP(std::string name, uint8_t cmd, uint8_t ops)
	{
		this->name = name;
		this->cmd = cmd;
		this->ops = ops;
	}
};

static void do_declare()
{
	std::string type = get_next_word(line_c);
	std::string name = get_next_word(line_c);
	to_lower(type);
	
	uint64_t addr;

	if(type == "const")
	{
		std::string var = get_data(get_next_word(line_c));
		uint64_t var_l = var.length();

		if(var_l <= 0xff) {
			put_uint8(24);
			put_uint8(var_l);
			cursor_prog += 2;
		} else if(var_l <= 0xffff) {
			put_uint8(25);
			put_uint16(var_l);
			cursor_prog += 3;
		} else if(var_l <= 0xffffffff) {
			put_uint8(26);
			put_uint32(var_l);
			cursor_prog += 5;
		} else {
			put_uint8(27);
			put_uint64(var_l);
			cursor_prog += 9;
		}

		addr = cursor_prog;
		cursor_prog += var_l;

		out->write(var.c_str(), var.length());
	}

	else if(type == "global")
	{
		addr = cursor_glob;
		cursor_glob += get_uint(get_next_word(line_c));
	}

	else if(type == "label")
	{
		addr = cursor_prog;
	}

	else if(type == "def")
	{
		addr = get_uint(get_next_word(line_c));
	}

	else
	{
		throw "bad type";
	}
		
	labels[name] = addr;
}

static void do_sload()
{
	uint64_t reg = get_uint(get_next_word(line_c));
	uint64_t val = get_uint(get_next_word(line_c));

	if(reg > 255) {
		throw "register index \"" + std::to_string(reg) + "\" is too high (max 255)";
	}

	if(val <= 0xff) {
		put_uint8(28);
		put_uint8(reg);
		put_uint8(val);
		cursor_prog += 3;
	} else if(val <= 0xffff) {
		put_uint8(29);
		put_uint8(reg);
		put_uint16(val);
		cursor_prog += 4;
	} else if(val <= 0xffffffff) {
		put_uint8(30);
		put_uint8(reg);
		put_uint32(val);
		cursor_prog += 6;
	} else {
		put_uint8(31);
		put_uint8(reg);
		put_uint64(val);
		cursor_prog += 10;
	}
}

static bool do_simple(std::string word)
{
	const SIOP ops[] =
	{
		SIOP("stop", 0, 0),
		SIOP("break", 1, 0),
		SIOP("goto", 2, 4),
		SIOP("equal", 3, 3),

		SIOP("and", 4, 3),
		SIOP("or", 5, 3),
		SIOP("xor", 6, 3),
		SIOP("not", 7, 2),

		SIOP("gthan-u", 8, 3),
		SIOP("gthan", 9, 3),
		SIOP("lthan-u", 10, 3),
		SIOP("lthan", 11, 3),

		SIOP("mul-u", 12, 3),
		SIOP("mul", 13, 3),
		SIOP("div-u", 14, 4),
		SIOP("div", 15, 4),

		SIOP("add", 16, 3),
		SIOP("sub", 17, 3),
		SIOP("shiftr", 18, 3),
		SIOP("shiftl", 19, 3),

		SIOP("bit-and", 20, 3),
		SIOP("bit-or", 21, 3),
		SIOP("bit-xor", 22, 3),
		SIOP("bit-not", 23, 2),

		// not implemented
	
		SIOP("load-8", 32, 2),
		SIOP("load-16", 33, 2),
		SIOP("load-32", 34, 2),
		SIOP("load-64", 35, 2),

		SIOP("store-8", 36, 2),
		SIOP("store-16", 37, 2),
		SIOP("store-32", 38, 2),
		SIOP("store-64", 39, 2),

		SIOP("f-add", 40, 3),
		SIOP("f-sub", 41, 3),
		SIOP("f-mul", 42, 3),
		SIOP("f-div", 43, 3),

		SIOP("d-add", 44, 3),
		SIOP("d-sub", 45, 3),
		SIOP("d-mul", 46, 3),
		SIOP("d-div", 47, 3),

		SIOP("f-sqrt", 48, 2),
		SIOP("f-equal", 49, 3),
		SIOP("f-gthan", 50, 3),
		SIOP("f-lthan", 51, 3),

		SIOP("d-sqrt", 52, 2),
		SIOP("d-equal", 53, 3),
		SIOP("d-gthan", 54, 3),
		SIOP("d-lthan", 55, 3),

		SIOP("f-to-uint", 56, 2),
		SIOP("f-to-int", 57, 2),
		SIOP("uint-to-f", 58, 2),
		SIOP("int-to-f", 59, 2),

		SIOP("d-to-uint", 60, 2),
		SIOP("d-to-int", 61, 2),
		SIOP("uint-to-d", 62, 2),
		SIOP("int-to-d", 63, 2),
	};

	for(const SIOP& op : ops)
	{
		if(op.name == word)
		{
			put_uint8(op.cmd);

			for(uint8_t i = 0; i < op.ops; i++)
			{
				uint64_t val = get_uint(get_next_word(line_c));

				if(val > 255) {
					throw "register index \"" + std::to_string(val) + "\" is too high (max 255)";
				}

				put_uint8(val);
			}

			cursor_prog += 1 + op.ops;

			return true;
		}
	}

	return false;
}

int main(int argc, const char** argv)
{
	if(argc < 2) return 1;
	
	std::string fn = argv[1];
	std::ifstream in(fn);
	std::ofstream file(fn + ".bin");
	size_t lineno = 0;

	out = &file;

	try
	{
		while(std::getline(in, line))
		{
			lineno += 1;

			line_c = line.c_str();
			std::string word = get_next_word(line_c);
			
			to_lower(word);
	
			if(word == "") {
				continue;
			}

			if(word[0] == '-') {
				continue;
			}
	
			if(word == "declare") {
				do_declare();
				continue;
			}

			if(word == "sload") {
				do_sload();
				continue;
			}

			if(do_simple(word)) {
				continue;
			}
				
			throw "syntax error: \"" + word + "\" not defined";
		}
	}

	catch(char const*& e)
	{
		std::cout << "error on line " << lineno << ": " << e << std::endl;
	}

	catch(std::string& e)
	{
		std::cout << "error on line " << lineno << ": " << e << std::endl;
	}

	file.close();
}

