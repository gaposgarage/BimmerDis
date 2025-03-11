/*
Overview:
This program is a disassembler for the "Beschreibungssprache für Steuergeräte" – BMW’s proprietary control unit description language used within the EDIABAS diagnostic system.
The language is used to create ECU description files (SGBD files, often distributed as *.PRG files) that define diagnostic jobs (e.g., reading error memory or sensor values).
EDIABAS (Electronic Diagnostic Basic System), developed by Softing on behalf of BMW, runs a virtual machine that interprets these diagnostic scripts.
This tool reads a BEST file from a given directory, decrypts its binary content (by XOR-ing every byte with 0xF7), and outputs a human-readable disassembly.
*/

#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <vector>
#include <string>
#include <map>
#include <set>
#include <algorithm>
#include <stdexcept>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <cctype>

using namespace std;
namespace fs = std::filesystem;

//---------------------------------------------------------------------
// Data structures

struct OpArg {
	uint8_t oa;
	string name;
	OpArg(uint8_t oa, const string& name) : oa(oa), name(name) {}
};

struct OpCode {
	uint8_t oc;
	string pneumonic;
	bool arg0IsNearAddress;
	OpCode(uint8_t oc, const string& pneumonic, bool arg0IsNearAddress = false)
		: oc(oc), pneumonic(pneumonic), arg0IsNearAddress(arg0IsNearAddress) {
	}
};

enum OpAddrMode : uint8_t {
	None = 0,
	RegS = 1,
	RegAB = 2,
	RegI = 3,
	RegL = 4,
	Imm8 = 5,
	Imm16 = 6,
	Imm32 = 7,
	ImmStr = 8,
	IdxImm = 9,
	IdxReg = 10,
	IdxRegImm = 11,
	IdxImmLenImm = 12,
	IdxImmLenReg = 13,
	IdxRegLenImm = 14,
	IdxRegLenReg = 15
};

//---------------------------------------------------------------------
// Global tables

vector<OpArg> oaList = {
	{0x00, "B0"}, {0x01, "B1"}, {0x02, "B2"}, {0x03, "B3"},
	{0x04, "B4"}, {0x05, "B5"}, {0x06, "B6"}, {0x07, "B7"},
	{0x08, "B8"}, {0x09, "B9"}, {0x0A, "BA"}, {0x0B, "BB"},
	{0x0C, "BC"}, {0x0D, "BD"}, {0x0E, "BE"}, {0x0F, "BF"},
	{0x10, "I0"}, {0x11, "I1"}, {0x12, "I2"}, {0x13, "I3"},
	{0x14, "I4"}, {0x15, "I5"}, {0x16, "I6"}, {0x17, "I7"},
	{0x18, "L0"}, {0x19, "L1"}, {0x1A, "L2"}, {0x1B, "L3"},
	{0x1C, "S0"}, {0x1D, "S1"}, {0x1E, "S2"}, {0x1F, "S3"},
	{0x20, "S4"}, {0x21, "S5"}, {0x22, "S6"}, {0x23, "S7"},
	{0x24, "F0"}, {0x25, "F1"}, {0x26, "F2"}, {0x27, "F3"},
	{0x28, "F4"}, {0x29, "F5"}, {0x2A, "F6"}, {0x2B, "F7"},
	{0x2C, "S8"}, {0x2D, "S9"}, {0x2E, "SA"}, {0x2F, "SB"},
	{0x30, "SC"}, {0x31, "SD"}, {0x32, "SE"}, {0x33, "SF"},
	{0x80, "A0"}, {0x81, "A1"}, {0x82, "A2"}, {0x83, "A3"},
	{0x84, "A4"}, {0x85, "A5"}, {0x86, "A6"}, {0x87, "A7"},
	{0x88, "A8"}, {0x89, "A9"}, {0x8A, "AA"}, {0x8B, "AB"},
	{0x8C, "AC"}, {0x8D, "AD"}, {0x8E, "AE"}, {0x8F, "AF"},
	{0x90, "I8"}, {0x91, "I9"}, {0x92, "IA"}, {0x93, "IB"},
	{0x94, "IC"}, {0x95, "ID"}, {0x96, "IE"}, {0x97, "IF"},
	{0x98, "L4"}, {0x99, "L5"}, {0x9A, "L6"}, {0x9B, "L7"}
};

vector<OpCode> ocList = {
	{0x00, "move"},
	{0x01, "clear"},
	{0x02, "comp"},
	{0x03, "subb"},
	{0x04, "adds"},
	{0x05, "mult"},
	{0x06, "divs"},
	{0x07, "and"},
	{0x08, "or"},
	{0x09, "xor"},
	{0x0A, "not"},
	{0x0B, "jump", true},
	{0x0C, "jtsr", true},
	{0x0D, "ret"},
	{0x0E, "jc", true},
	{0x0F, "jae", true},
	{0x10, "jz", true},
	{0x11, "jnz", true},
	{0x12, "jv", true},
	{0x13, "jnv", true},
	{0x14, "jmi", true},
	{0x15, "jpl", true},
	{0x16, "clrc"},
	{0x17, "setc"},
	{0x18, "asr"},
	{0x19, "lsl"},
	{0x1A, "lsr"},
	{0x1B, "asl"},
	{0x1C, "nop"},
	{0x1D, "eoj"},
	{0x1E, "push"},
	{0x1F, "pop"},
	{0x20, "scmp"},
	{0x21, "scat"},
	{0x22, "scut"},
	{0x23, "slen"},
	{0x24, "spaste"},
	{0x25, "serase"},
	{0x26, "xconnect"},
	{0x27, "xhangup"},
	{0x28, "xsetpar"},
	{0x29, "xawlen"},
	{0x2A, "xsend"},
	{0x2B, "xsendf"},
	{0x2C, "xrequf"},
	{0x2D, "xstopf"},
	{0x2E, "xkeyb"},
	{0x2F, "xstate"},
	{0x30, "xboot"},
	{0x31, "xreset"},
	{0x32, "xtype"},
	{0x33, "xvers"},
	{0x34, "ergb"},
	{0x35, "ergw"},
	{0x36, "ergd"},
	{0x37, "ergi"},
	{0x38, "ergr"},
	{0x39, "ergs"},
	{0x3A, "a2flt"},
	{0x3B, "fadd"},
	{0x3C, "fsub"},
	{0x3D, "fmul"},
	{0x3E, "fdiv"},
	{0x3F, "ergy"},
	{0x40, "enewset"},
	{0x41, "etag", true},
	{0x42, "xreps"},
	{0x43, "gettmr"},
	{0x44, "settmr"},
	{0x45, "sett"},
	{0x46, "clrt"},
	{0x47, "jt", true},
	{0x48, "jnt", true},
	{0x49, "addc"},
	{0x4A, "subc"},
	{0x4B, "break"},
	{0x4C, "clrv"},
	{0x4D, "eerr"},
	{0x4E, "popf"},
	{0x4F, "pushf"},
	{0x50, "atsp"},
	{0x51, "swap"},
	{0x52, "setspc"},
	{0x53, "srevrs"},
	{0x54, "stoken"},
	{0x55, "parb"},
	{0x56, "parw"},
	{0x57, "parl"},
	{0x58, "pars"},
	{0x59, "fclose"},
	{0x5A, "jg", true},
	{0x5B, "jge", true},
	{0x5C, "jl", true},
	{0x5D, "jle", true},
	{0x5E, "ja", true},
	{0x5F, "jbe", true},
	{0x60, "fopen"},
	{0x61, "fread"},
	{0x62, "freadln"},
	{0x63, "fseek"},
	{0x64, "fseekln"},
	{0x65, "ftell"},
	{0x66, "ftellln"},
	{0x67, "a2fix"},
	{0x68, "fix2flt"},
	{0x69, "parr"},
	{0x6A, "test"},
	{0x6B, "wait"},
	{0x6C, "date"},
	{0x6D, "time"},
	{0x6E, "xbatt"},
	{0x6F, "tosp"},
	{0x70, "xdownl"},
	{0x71, "xgetport"},
	{0x72, "xignit"},
	{0x73, "xloopt"},
	{0x74, "xprog"},
	{0x75, "xraw"},
	{0x76, "xsetport"},
	{0x77, "xsireset"},
	{0x78, "xstoptr"},
	{0x79, "fix2hex"},
	{0x7A, "fix2dez"},
	{0x7B, "tabset"},
	{0x7C, "tabseek"},
	{0x7D, "tabget"},
	{0x7E, "strcat"},
	{0x7F, "pary"},
	{0x80, "parn"},
	{0x81, "ergc"},
	{0x82, "ergl"},
	{0x83, "tabline"},
	{0x84, "xsendr"},
	{0x85, "xrecv"},
	{0x86, "xinfo"},
	{0x87, "flt2a"},
	{0x88, "setflt"},
	{0x89, "cfgig"},
	{0x8A, "cfgsg"},
	{0x8B, "cfgis"},
	{0x8C, "a2y"},
	{0x8D, "xparraw"},
	{0x8E, "hex2y"},
	{0x8F, "strcmp"},
	{0x90, "strlen"},
	{0x91, "y2bcd"},
	{0x92, "y2hex"},
	{0x93, "shmset"},
	{0x94, "shmget"},
	{0x95, "ergsysi"},
	{0x96, "flt2fix"},
	{0x97, "iupdate"},
	{0x98, "irange"},
	{0x99, "iincpos"},
	{0x9A, "tabseeku"},
	{0x9B, "flt2y4"},
	{0x9C, "flt2y8"},
	{0x9D, "y42flt"},
	{0x9E, "y82flt"},
	{0x9F, "plink"},
	{0xA0, "pcall"},
	{0xA1, "fcomp"},
	{0xA2, "plinkv"},
	{0xA3, "ppush"},
	{0xA4, "ppop"},
	{0xA5, "ppushflt"},
	{0xA6, "ppopflt"},
	{0xA7, "ppushy"},
	{0xA8, "ppopy"},
	{0xA9, "pjtsr"},
	{0xAA, "tabsetex"},
	{0xAB, "ufix2dez"},
	{0xAC, "generr"},
	{0xAD, "ticks"},
	{0xAE, "waitex"},
	{0xAF, "xopen"},
	{0xB0, "xclose"},
	{0xB1, "xcloseex"},
	{0xB2, "xswitch"},
	{0xB3, "xsendex"},
	{0xB4, "xrecvex"},
	{0xB5, "ssize"},
	{0xB6, "tabcols"},
	{0xB7, "tabrows"},
	{0x0E, "jb", true},
	{0x0F, "jnc", true}
};

//---------------------------------------------------------------------
// Global variables

// Mapping from job name to its list of comment strings.
map<string, vector<string>> comments;

// Global output stream pointer (set to the current ofstream).
ofstream* output = nullptr;

//---------------------------------------------------------------------
// Utility functions

// Checks if a byte is printable (or one of the allowed whitespace characters).
bool isPrintable(unsigned char b) {
	return (b == 9 || b == 10 || b == 13 || (b >= 32 && b < 127));
}

// Reads 'count' bytes from 'fs' into 'buffer' (starting at offset) and XORs each byte with 0xF7.
void readAndDecryptBytes(ifstream& fs, char* buffer, int offset, int count) {
	fs.read(buffer + offset, count);
	if (fs.gcount() != count)
		throw runtime_error("Failed to read required bytes from file.");
	for (int i = offset; i < offset + count; i++) {
		buffer[i] ^= 0xF7;
	}
}

// Reads 4 bytes from 'fs' (without decryption) and returns the little-endian int32 value.
int32_t readInt32(ifstream& fs) {
	char buf[4];
	fs.read(buf, 4);
	int32_t value = ((unsigned char)buf[0]) |
		(((unsigned char)buf[1]) << 8) |
		(((unsigned char)buf[2]) << 16) |
		(((unsigned char)buf[3]) << 24);
	return value;
}

// Helper functions to retrieve a 32-bit or 16-bit integer from a buffer (little-endian).
int32_t getInt32(const vector<char>& buffer, size_t offset) {
	int32_t value = ((unsigned char)buffer[offset]) |
		(((unsigned char)buffer[offset + 1]) << 8) |
		(((unsigned char)buffer[offset + 2]) << 16) |
		(((unsigned char)buffer[offset + 3]) << 24);
	return value;
}

int16_t getInt16(const vector<char>& buffer, size_t offset) {
	int16_t value = ((unsigned char)buffer[offset]) |
		(((unsigned char)buffer[offset + 1]) << 8);
	return value;
}

//---------------------------------------------------------------------
// disassembleOpArg: Reads an operand argument from 'fs' based on the addressing mode.
string disassembleOpArg(ifstream& fs, OpAddrMode mode) {
	switch (mode) {
	case None:
		return "";
	case RegS:
	case RegAB:
	case RegI:
	case RegL: {
		char byteVal;
		readAndDecryptBytes(fs, &byteVal, 0, 1);
		uint8_t reg = (unsigned char)byteVal;
		auto it = find_if(oaList.begin(), oaList.end(), [reg](const OpArg& o) {
			return o.oa == reg;
			});
		if (it != oaList.end())
			return it->name;
		else
			throw runtime_error("Unknown register operand");
	}
	case Imm8: {
		char byteVal;
		readAndDecryptBytes(fs, &byteVal, 0, 1);
		uint8_t val = (unsigned char)byteVal;
		if (isPrintable(val)) {
			string v;
			if (val == '\t')
				v = "\\t";
			else if (val == '\r')
				v = "\\r";
			else if (val == '\n')
				v = "\\n";
			else
				v = string(1, (char)val);
			return "#'" + v + "'";
		}
		else {
			ostringstream oss;
			oss << "#$" << uppercase << hex << (int)val << ".B";
			return oss.str();
		}
	}
	case Imm16: {
		char buf[2];
		readAndDecryptBytes(fs, buf, 0, 2);
		int16_t val = ((unsigned char)buf[0]) | (((unsigned char)buf[1]) << 8);
		ostringstream oss;
		oss << "#$" << uppercase << hex << (int)val << ".I";
		return oss.str();
	}
	case Imm32: {
		char buf[4];
		readAndDecryptBytes(fs, buf, 0, 4);
		int32_t val = ((unsigned char)buf[0]) | (((unsigned char)buf[1]) << 8) |
			(((unsigned char)buf[2]) << 16) | (((unsigned char)buf[3]) << 24);
		ostringstream oss;
		oss << "#$" << uppercase << hex << val << ".L";
		return oss.str();
	}
	case ImmStr: {
		char buf[2];
		readAndDecryptBytes(fs, buf, 0, 2);
		int16_t slen = ((unsigned char)buf[0]) | (((unsigned char)buf[1]) << 8);
		vector<char> strBuf(slen);
		if (slen > 0)
			readAndDecryptBytes(fs, strBuf.data(), 0, slen);
		bool printable = true;
		for (int i = 0; i < slen - 1; i++) {
			if (!isPrintable((unsigned char)strBuf[i])) {
				printable = false;
				break;
			}
		}
		if (printable && strBuf[slen - 1] == 0) {
			return "\"" + string(strBuf.data(), slen - 1) + "\"";
		}
		else {
			ostringstream oss;
			oss << "{";
			for (int i = 0; i < slen; i++) {
				oss << "$" << uppercase << hex << setw(2) << setfill('0')
					<< (int)(unsigned char)strBuf[i] << ".B,";
			}
			string s = oss.str();
			if (!s.empty())
				s.pop_back(); // remove trailing comma
			s += "}";
			return s;
		}
	}
	case IdxImm: {
		char buf[3];
		readAndDecryptBytes(fs, buf, 0, 3);
		uint8_t reg = (unsigned char)buf[0];
		int16_t idx = ((unsigned char)buf[1]) | (((unsigned char)buf[2]) << 8);
		auto it = find_if(oaList.begin(), oaList.end(), [reg](const OpArg& o) { return o.oa == reg; });
		if (it == oaList.end())
			throw runtime_error("Unknown register in IdxImm");
		ostringstream oss;
		oss << it->name << "[#$" << uppercase << hex << idx << "]";
		return oss.str();
	}
	case IdxReg: {
		char buf[2];
		readAndDecryptBytes(fs, buf, 0, 2);
		uint8_t reg0 = (unsigned char)buf[0];
		uint8_t reg1 = (unsigned char)buf[1];
		auto it0 = find_if(oaList.begin(), oaList.end(), [reg0](const OpArg& o) { return o.oa == reg0; });
		auto it1 = find_if(oaList.begin(), oaList.end(), [reg1](const OpArg& o) { return o.oa == reg1; });
		if (it0 == oaList.end() || it1 == oaList.end())
			throw runtime_error("Unknown register in IdxReg");
		ostringstream oss;
		oss << it0->name << "[" << it1->name << "]";
		return oss.str();
	}
	case IdxRegImm: {
		char buf[4];
		readAndDecryptBytes(fs, buf, 0, 4);
		uint8_t reg0 = (unsigned char)buf[0];
		uint8_t reg1 = (unsigned char)buf[1];
		int16_t inc = ((unsigned char)buf[2]) | (((unsigned char)buf[3]) << 8);
		auto it0 = find_if(oaList.begin(), oaList.end(), [reg0](const OpArg& o) { return o.oa == reg0; });
		auto it1 = find_if(oaList.begin(), oaList.end(), [reg1](const OpArg& o) { return o.oa == reg1; });
		if (it0 == oaList.end() || it1 == oaList.end())
			throw runtime_error("Unknown register in IdxRegImm");
		ostringstream oss;
		oss << it0->name << "[" << it1->name << ",#$" << uppercase << hex << inc << "]";
		return oss.str();
	}
	case IdxImmLenImm: {
		char buf[5];
		readAndDecryptBytes(fs, buf, 0, 5);
		uint8_t reg = (unsigned char)buf[0];
		int16_t idx = ((unsigned char)buf[1]) | (((unsigned char)buf[2]) << 8);
		int16_t len = ((unsigned char)buf[3]) | (((unsigned char)buf[4]) << 8);
		auto it = find_if(oaList.begin(), oaList.end(), [reg](const OpArg& o) { return o.oa == reg; });
		if (it == oaList.end())
			throw runtime_error("Unknown register in IdxImmLenImm");
		ostringstream oss;
		oss << it->name << "[#$" << uppercase << hex << idx << "]#$" << len;
		return oss.str();
	}
	case IdxImmLenReg: {
		char buf[4];
		readAndDecryptBytes(fs, buf, 0, 4);
		uint8_t reg = (unsigned char)buf[0];
		int16_t idx = ((unsigned char)buf[1]) | (((unsigned char)buf[2]) << 8);
		uint8_t regLen = (unsigned char)buf[3];
		auto it = find_if(oaList.begin(), oaList.end(), [reg](const OpArg& o) { return o.oa == reg; });
		auto itLen = find_if(oaList.begin(), oaList.end(), [regLen](const OpArg& o) { return o.oa == regLen; });
		if (it == oaList.end() || itLen == oaList.end())
			throw runtime_error("Unknown register in IdxImmLenReg");
		ostringstream oss;
		oss << it->name << "[#$" << uppercase << hex << idx << "]" << itLen->name;
		return oss.str();
	}
	case IdxRegLenImm: {
		char buf[4];
		readAndDecryptBytes(fs, buf, 0, 4);
		uint8_t reg = (unsigned char)buf[0];
		uint8_t regIdx = (unsigned char)buf[1];
		int16_t len = ((unsigned char)buf[2]) | (((unsigned char)buf[3]) << 8);
		auto it = find_if(oaList.begin(), oaList.end(), [reg](const OpArg& o) { return o.oa == reg; });
		auto itIdx = find_if(oaList.begin(), oaList.end(), [regIdx](const OpArg& o) { return o.oa == regIdx; });
		if (it == oaList.end() || itIdx == oaList.end())
			throw runtime_error("Unknown register in IdxRegLenImm");
		ostringstream oss;
		oss << it->name << "[" << itIdx->name << "]#$" << uppercase << hex << len;
		return oss.str();
	}
	case IdxRegLenReg: {
		char buf[3];
		readAndDecryptBytes(fs, buf, 0, 3);
		uint8_t reg = (unsigned char)buf[0];
		uint8_t regIdx = (unsigned char)buf[1];
		uint8_t regLen = (unsigned char)buf[2];
		auto it = find_if(oaList.begin(), oaList.end(), [reg](const OpArg& o) { return o.oa == reg; });
		auto itIdx = find_if(oaList.begin(), oaList.end(), [regIdx](const OpArg& o) { return o.oa == regIdx; });
		auto itLen = find_if(oaList.begin(), oaList.end(), [regLen](const OpArg& o) { return o.oa == regLen; });
		if (it == oaList.end() || itIdx == oaList.end() || itLen == oaList.end())
			throw runtime_error("Unknown register in IdxRegLenReg");
		ostringstream oss;
		oss << it->name << "[" << itIdx->name << "]" << itLen->name;
		return oss.str();
	}
	default:
		throw runtime_error("Unsupported OpAddrMode");
	}
}

//---------------------------------------------------------------------
// disassembleJob: Disassembles the job code starting at jobAddress.
void disassembleJob(ifstream& fs, int jobAddress) {
	fs.seekg(jobAddress, ios::beg);
	char buf[2];
	bool foundFirstEoj = false;
	map<int, string> lines;
	set<int> labelSet;

	while (true) {
		int address = static_cast<int>(fs.tellg());
		readAndDecryptBytes(fs, buf, 0, 2);
		uint8_t opCodeVal = (unsigned char)buf[0];
		uint8_t opAddrMode = (unsigned char)buf[1];
		int opAddrMode0 = (opAddrMode & 0xF0) >> 4;
		int opAddrMode1 = (opAddrMode & 0x0F);

		auto it = find_if(ocList.begin(), ocList.end(), [opCodeVal](const OpCode& oc) { return oc.oc == opCodeVal; });
		if (it == ocList.end())
			throw runtime_error("Unknown OpCode");
		const OpCode& oc = *it;

		string arg0 = disassembleOpArg(fs, static_cast<OpAddrMode>(opAddrMode0));
		string arg1 = disassembleOpArg(fs, static_cast<OpAddrMode>(opAddrMode1));

		if (oc.arg0IsNearAddress && opAddrMode0 == Imm32) {
			int currentPos = static_cast<int>(fs.tellg());
			if (arg0.size() >= 4) {
				string hexStr = arg0.substr(2, arg0.size() - static_cast<size_t>(4));
				// Use stoul instead of stoi to avoid out_of_range error
				unsigned long offsetVal = stoul(hexStr, nullptr, 16);
				int labelAddress = currentPos + static_cast<int>(offsetVal);
				labelSet.insert(labelAddress);
				ostringstream oss;
				oss << "__" << setw(8) << setfill('0') << uppercase << hex << labelAddress;
				arg0 = oss.str();
			}
		}

		ostringstream lineStream;
		if (!arg0.empty()) {
			lineStream << left << setw(10) << oc.pneumonic << " ";
			if (!arg1.empty())
				lineStream << arg0 << "," << arg1;
			else
				lineStream << arg0;
		}
		else {
			lineStream << oc.pneumonic;
		}
		lines[address] = lineStream.str();

		if (opCodeVal == 0x1D) {
			if (foundFirstEoj)
				break;
			foundFirstEoj = true;
		}
		else {
			foundFirstEoj = false;
		}
	}

	// Output lines in sorted order. If an address is a jump label, output it with the label.
	for (const auto& pair : lines) {
		int addr = pair.first;
		if (labelSet.find(addr) != labelSet.end()) {
			ostringstream oss;
			oss << "__"
				<< setw(8) << setfill('0') << uppercase << hex << addr << ": ";
			// Reset fill to space for the following field
			oss << setw(8) << setfill(' ') << "" << pair.second;
			(*output) << oss.str() << "\n";
		}
		else {
			(*output) << setw(20) << "" << pair.second << "\n";
		}
	}
}

//---------------------------------------------------------------------
// dumpInfo: Reads and writes version, revision, and author information.
void dumpInfo(ifstream& fs, const vector<char>& buffer) {
	int infoOffset = getInt32(buffer, 0x94);
	fs.seekg(infoOffset, ios::beg);
	vector<char> infoBuffer(0x6C);
	readAndDecryptBytes(fs, infoBuffer.data(), 0, static_cast<int>(infoBuffer.size()));

	(*output) << ";BIP Version: ";
	(*output) << setw(2) << setfill('0') << uppercase << hex << (int)((unsigned char)infoBuffer[2]) << "."
		<< setw(2) << setfill('0') << (int)((unsigned char)infoBuffer[1]) << "."
		<< setw(2) << setfill('0') << (int)((unsigned char)infoBuffer[0]) << dec << "\n";

	int16_t rev1 = ((unsigned char)infoBuffer[0x06]) | (((unsigned char)infoBuffer[0x07]) << 8);
	int16_t rev2 = ((unsigned char)infoBuffer[0x04]) | (((unsigned char)infoBuffer[0x05]) << 8);
	(*output) << ";Revision Number: " << rev1 << "." << rev2 << "\n";

	string lastChanged(infoBuffer.data() + 0x48, 0x24);
	lastChanged.erase(find(lastChanged.begin(), lastChanged.end(), '\0'), lastChanged.end());
	(*output) << ";Last Changed: " << lastChanged << "\n";

	string by(infoBuffer.data() + 0x08, 0x40);
	by.erase(find(by.begin(), by.end(), '\0'), by.end());
	(*output) << ";By: " << by << "\n";

	int32_t pkgVersion = ((unsigned char)infoBuffer[0x68]) | (((unsigned char)infoBuffer[0x69]) << 8) |
		(((unsigned char)infoBuffer[0x6A]) << 16) | (((unsigned char)infoBuffer[0x6B]) << 24);
	(*output) << ";Package Version: " << setw(8) << setfill('0') << uppercase << hex << pkgVersion << dec << "\n";
	(*output) << "\n";
}

//---------------------------------------------------------------------
// dumpDescription: Reads the description/comment block.
void dumpDescription(ifstream& fs, const vector<char>& buffer) {
	int startOffset = getInt32(buffer, 0x90);
	if (startOffset == -1)
		return;
	fs.seekg(startOffset, ios::beg);

	vector<string> commentList;
	string previousJobName = "";

	int numBytes = readInt32(fs);
	vector<char> recordBuffer(1100, 0);
	int recordOffset = 0;
	for (int i = 0; i < numBytes; i++) {
		char byteVal;
		readAndDecryptBytes(fs, &byteVal, 0, 1);
		recordBuffer[recordOffset++] = byteVal;
		if (recordOffset >= 1098)
			recordBuffer[recordOffset++] = 10; // newline
		if (recordBuffer[recordOffset - 1] == 10) {
			recordBuffer[recordOffset] = 0;
			string comment(recordBuffer.data(), recordOffset - 1);
			if (comment.find("JOBNAME:") == 0) {
				comments[previousJobName] = commentList;
				commentList.clear();
				previousJobName = comment.substr(8);
			}
			commentList.push_back(comment);
			recordOffset = 0;
		}
	}
	comments[previousJobName] = commentList;

	if (comments.find("") != comments.end()) {
		for (const auto& c : comments[""]) {
			(*output) << ";" << c << "\n";
		}
		(*output) << "\n";
	}
}

//---------------------------------------------------------------------
// dumpUses: Reads and writes the "uses" section.
void dumpUses(ifstream& fs, const vector<char>& buffer) {
	int usesOffset = getInt32(buffer, 0x7C);
	fs.seekg(usesOffset, ios::beg);
	int usesCount = readInt32(fs);
	for (int i = 0; i < usesCount; i++) {
		vector<char> usesBuffer(0x100, 0);
		readAndDecryptBytes(fs, usesBuffer.data(), 0, static_cast<int>(usesBuffer.size()));
		string usesStr(usesBuffer.data());
		usesStr.erase(find(usesStr.begin(), usesStr.end(), '\0'), usesStr.end());
		(*output) << ";Uses: " << usesStr << "\n";
	}
	(*output) << "\n";
}

//---------------------------------------------------------------------
// dumpSsize: Reads and writes the "#SSIZE" line.
void dumpSsize(ifstream& fs, const vector<char>& buffer) {
	int ssize = getInt32(buffer, 0x18);
	if (ssize != 0) {
		(*output) << "#SSIZE " << ssize << "\n\n";
	}
}

//---------------------------------------------------------------------
// dumpTables: Reads table definitions and writes table header and lines.
void dumpTables(ifstream& fs, const vector<char>& buffer) {
	int tableOffset = getInt32(buffer, 0x84);
	fs.seekg(tableOffset, ios::beg);

	char tableCountBuffer[4];
	readAndDecryptBytes(fs, tableCountBuffer, 0, 4);
	int tableCount = ((unsigned char)tableCountBuffer[0]) |
		(((unsigned char)tableCountBuffer[1]) << 8) |
		(((unsigned char)tableCountBuffer[2]) << 16) |
		(((unsigned char)tableCountBuffer[3]) << 24);

	ostringstream sb;
	for (int i = 0; i < tableCount; i++) {
		vector<char> tableBuffer(0x50, 0);
		readAndDecryptBytes(fs, tableBuffer.data(), 0, static_cast<int>(tableBuffer.size()));
		string name(tableBuffer.data(), 0x40);
		name.erase(find(name.begin(), name.end(), '\0'), name.end());

		int tableColumnOffset = ((unsigned char)tableBuffer[0x40]) |
			(((unsigned char)tableBuffer[0x41]) << 8) |
			(((unsigned char)tableBuffer[0x42]) << 16) |
			(((unsigned char)tableBuffer[0x43]) << 24);
		int tableColumnCount = ((unsigned char)tableBuffer[0x48]) |
			(((unsigned char)tableBuffer[0x49]) << 8) |
			(((unsigned char)tableBuffer[0x4A]) << 16) |
			(((unsigned char)tableBuffer[0x4B]) << 24);
		int tableRowCount = ((unsigned char)tableBuffer[0x4C]) |
			(((unsigned char)tableBuffer[0x4D]) << 8) |
			(((unsigned char)tableBuffer[0x4E]) << 16) |
			(((unsigned char)tableBuffer[0x4F]) << 24);

		(*output) << "TBEG \"" << name << "\"\n";

		auto savedPos = fs.tellg();
		fs.seekg(tableColumnOffset, ios::beg);

		sb.str("");
		sb.clear();
		sb << "HEAD ";
		for (int j = 0; j < tableColumnCount; j++) {
			string item;
			for (int k = 0; k < 1024; k++) {
				char ch;
				readAndDecryptBytes(fs, &ch, 0, 1);
				if (ch == 0)
					break;
				item.push_back(ch);
			}
			sb << "\"" << item << "\", ";
		}
		string headerLine = sb.str();
		if (headerLine.size() >= 2)
			headerLine.erase(headerLine.size() - static_cast<size_t>(2), static_cast<size_t>(2));
		(*output) << headerLine << "\n";

		for (int j = 0; j < tableRowCount; j++) {
			sb.str("");
			sb.clear();
			sb << "LINE ";
			for (int k = 0; k < tableColumnCount; k++) {
				string item;
				for (int l = 0; l < 1024; l++) {
					char ch;
					readAndDecryptBytes(fs, &ch, 0, 1);
					if (ch == 0)
						break;
					item.push_back(ch);
				}
				sb << "\"" << item << "\", ";
			}
			string lineStr = sb.str();
			if (lineStr.size() >= 2)
				lineStr.erase(lineStr.size() - static_cast<size_t>(2), static_cast<size_t>(2));
			(*output) << lineStr << "\n";
		}

		(*output) << "TEND\n\n";
		fs.seekg(savedPos, ios::beg);
	}

	(*output) << "\n";
}

//---------------------------------------------------------------------
// dumpJobs: Reads each job entry, outputs its header, and disassembles its code.
void dumpJobs(ifstream& fs, const vector<char>& buffer) {
	int jobListOffset = getInt32(buffer, 0x88);
	fs.seekg(jobListOffset, ios::beg);
	int numJobs = readInt32(fs);
	for (int i = 0; i < numJobs; i++) {
		vector<char> jobBuffer(0x44, 0);
		readAndDecryptBytes(fs, jobBuffer.data(), 0, static_cast<int>(jobBuffer.size()));
		string jobNameString(jobBuffer.data(), 0x40);
		jobNameString.erase(find(jobNameString.begin(), jobNameString.end(), '\0'), jobNameString.end());
		int jobAddress = ((unsigned char)jobBuffer[0x40]) |
			(((unsigned char)jobBuffer[0x41]) << 8) |
			(((unsigned char)jobBuffer[0x42]) << 16) |
			(((unsigned char)jobBuffer[0x43]) << 24);
		(*output) << jobNameString << "#\n";
		if (comments.find(jobNameString) != comments.end()) {
			for (const auto& c : comments[jobNameString]) {
				(*output) << ";" << setw(4) << "" << c << "\n";
			}
			(*output) << ";\n";
		}
		auto fsPos = fs.tellg();
		disassembleJob(fs, jobAddress);
		fs.seekg(fsPos, ios::beg);
		(*output) << "\n\n";
	}
}

//---------------------------------------------------------------------
// Main entry point: Finds the input file(s), opens each, and processes them.
int main(int argc, char* argv[]) {
	try {
	if (argc != 2) {
		cerr << "Usage: BESTDIS <BEST file directory>" << endl;
		return -1;
	}

	fs::path inputPath(argv[1]);
	fs::path dirName = inputPath.parent_path();
	if (dirName.empty()) {
		cerr << "Specify file with path name" << endl;
		return -1;
	}

	vector<fs::path> files;
	string pattern = inputPath.filename().string();
	for (const auto& p : fs::directory_iterator(dirName)) {
		if (p.path().filename() == pattern) {
			files.push_back(p.path());
		}
	}

	for (const auto& file : files) {
		string inputFile = file.string();
		string inputExtension = file.extension().string();
		string outputExtension = ".b1v";
		// Convert extension to lowercase and compare with ".grp"
		string lower_ext = inputExtension;
		transform(lower_ext.begin(), lower_ext.end(), lower_ext.begin(), ::tolower);
		if (lower_ext == ".grp")
			outputExtension = ".b1g";

		fs::path outputPath = file.parent_path() / file.stem();
		outputPath += outputExtension;
		ofstream outStream(outputPath, ios::binary);
		if (!outStream) {
			cerr << "Failed to open output file: " << outputPath.string() << endl;
			continue;
		}
		output = &outStream;

		comments.clear();

		ifstream inStream(inputFile, ios::binary);
		if (!inStream) {
			cerr << "Failed to open input file: " << inputFile << endl;
			continue;
		}

		vector<char> buffer(0x9C, 0);
		inStream.read(buffer.data(), buffer.size());

		dumpInfo(inStream, buffer);
		dumpDescription(inStream, buffer);
		dumpUses(inStream, buffer);
		dumpSsize(inStream, buffer);
		dumpTables(inStream, buffer);
		dumpJobs(inStream, buffer);

		inStream.close();
		outStream.close();
	}
	}
	catch (const exception& ex) {
		cerr << "Error: " << ex.what() << endl;
		return -1073740791;
	}

	return 0;
}