#pragma once
#include <string>
#include <vector>
#include <memory>
#include <fstream>

namespace JPG {
	using byte = unsigned char;
	using uint = unsigned int;

	// SOI (Start of Image) marker
	const byte SOI      = 0xd8;   // Start Of Image
									 
	// App markers (can ignore just read the bytes off them)		 
	const byte APP0     = 0xe0;
	const byte APP1		= 0xe1;
	const byte APP2		= 0xe2;
	const byte APP3		= 0xe3;
	const byte APP4		= 0xe4;
	const byte APP5		= 0xe5;
	const byte APP6		= 0xe6;
	const byte APP7		= 0xe7;
	const byte APP8		= 0xe8;
	const byte APP9		= 0xe9;
	const byte APP10	= 0xea;
	const byte APP11	= 0xeb;
	const byte APP12	= 0xec;
	const byte APP13	= 0xed;
	const byte APP14	= 0xee;
	const byte APP15    = 0xef;
			
	// SOF (Start of Frame) markers
	const byte SOF0     = 0xc0; // baseline
	const byte SOF1     = 0xc1;
	const byte SOF2     = 0xc2; // progressive I think
	const byte SOF3     = 0xc3;
	const byte SOF5     = 0xc5;
	const byte SOF6     = 0xc6;
	const byte SOF7     = 0xc7;
	const byte SOF9		= 0xc9;
	const byte SOF10    = 0xca;
	const byte SOF11	= 0xcb;
	const byte SOF13    = 0xcd;
	const byte SOF14	= 0xce;
	const byte SOF15	= 0xcf;

	// table markers
	const byte DHT      = 0xc4; // define huffman tables
	const byte DQT      = 0xdb; // define quantization tables
	const byte SOS      = 0xda; // start of scan (Start of huffman coded bitstream)
	const byte DAC      = 0xcc; // <--- almost never used
	
	// JPG markers (They should never occur in a JPG File if they do then process them as an error)
	const byte JPG      = 0xc8;
	const byte JPG0     = 0xf0;
	const byte JPG1     = 0xf1;
	const byte JPG2     = 0xf2;	
	const byte JPG3     = 0xf3;
	const byte JPG4     = 0xf4;
	const byte JPG5     = 0xf5;
	const byte JPG6     = 0xf6;
	const byte JPG7     = 0xf7;
	const byte JPG8     = 0xf8;
	const byte JPG9     = 0xf9;
	const byte JPG10    = 0xfa;
	const byte JPG11    = 0xfb;
	const byte JPG12    = 0xfc;
	const byte JPG13    = 0xfd;

	// Misc markers
	const byte DNL      = 0xdc;
	const byte DRI      = 0xdd; // define restart interval
	const byte DHP      = 0xde;
	const byte EXP      = 0xdf;
	const byte TEM      = 0x01;
	const byte COM      = 0xfe;

	// Restart interval markers
	const byte RST0     = 0xd0;
	const byte RST1     = 0xd1;
	const byte RST2     = 0xd2;
	const byte RST3     = 0xd3;
	const byte RST4     = 0xd4;
	const byte RST5     = 0xd5;
	const byte RST6     = 0xd6;
	const byte RST7     = 0xd7;

	// EOI (End of Image) marker
	const byte EOI      = 0xd9;


	// JPG Decoder
	const byte MCUMap[] = {
		0, 1, 8, 16, 9, 2, 3, 10,
		17, 24, 32, 25, 18, 11, 4, 5,
		12, 19, 26, 33, 40, 48, 41, 34,
		27, 20, 13, 6, 7, 14, 21, 28,
		35, 42, 49, 56, 57, 50, 43, 36,
		29, 22, 15, 23, 30, 37, 44, 51,
		58, 59, 52, 45, 38, 31, 39, 46,
		53, 60, 61, 54, 47, 55, 62, 63
	};

	struct ColorComponent {
		byte HSF = 1;
		byte VSF = 1;
		byte quantizationTableID = 0;
		byte huffmanDCTableID = 0;
		byte huffmanACTableID = 0;
		bool used = false;
	};

	struct QuantizationTable {
		uint table[64];
		bool set = false;
	};

	struct HuffmanTable {
		std::vector<std::vector<byte>> symbols = std::vector<std::vector<byte>>(16);
		std::vector<std::vector<uint>> codes = std::vector<std::vector<uint>>(16);
		bool set = false;
	};

	struct MCU {
		int y[64] = { 0 };
		int cb[64] = { 0 };
		int cr[64] = { 0 };

		int* operator[](const uint index) {
			switch (index) {
			case 0:
				return y;
			case 1:
				return cb;
			case 2:
				return cr;
			default:
				return nullptr;
			}
		}
	};

	struct JPGFile {
		QuantizationTable qtTables[4] = { 0 };
		HuffmanTable huffmanDCTables[4];
		HuffmanTable huffmanACTables[4];
		ColorComponent components[3] = { 0 };

		byte sofType = 0;
		uint width = 0;
		uint mcuWidth = 0;
		uint height = 0;
		uint mcuHeight = 0;
		byte numComponents = 0;

		byte startOfSelection = 0;
		byte endOfSelection = 0;
		byte successiveApproximationHigh = 0;
		byte successiveApproximationLow = 0;

		std::vector<byte> huffmanBitstream;

		uint restartInterval = 0;

		bool zerobased = false;
	};

	class JPGDecoder {
	public:
		static std::unique_ptr<JPGFile> ReadJPG(const std::string& filename);
		static std::unique_ptr<MCU[]> DecodeJPG(JPGFile& contents);
		static void WriteBMPFromJPG(const JPGFile& contents, const MCU mcus[], const std::string& fileName);
	private:
		static void ProcessAPPN(std::ifstream& file, JPGFile& jpgContents);
		static void ProcessQuantizationTable(std::ifstream& file, JPGFile& jpgContents);
		static void ProcessHuffmanTable(std::ifstream& file, JPGFile& jpgContents);
		static void ProcessStartOfFrame(std::ifstream& file, JPGFile& jpgContents);
		static void ProcessRestartInterval(std::ifstream& file, JPGFile& jpgContents);
		static void ProccesStartOfScan(std::ifstream& file, JPGFile& jpgContents);
		static void ProcessComment(std::ifstream& file, JPGFile& jpgContents);
	private:
		static void DecodeHuffmanData(MCU mcus[], JPGFile& contents);
		static void DequantizeMCUs(MCU mcus[], JPGFile& contents);
		static void InverseDiscreteCosineTransform(MCU mcus[], JPGFile& contents);
		static void YCbCrToRGB(MCU mcus[], JPGFile& contents);
		static void GenerateHuffmanCodes(HuffmanTable& hTable);
		static void DecodeMCUComponent(class BitReader& b, HuffmanTable& huffmanDCTable, HuffmanTable& huffmanACTable, int MCUComponent[64], int& prevCoeff);
	};
}

// TODO :
// Add support for restart intervals in HCB
// Add error handling for invalid markers
// Add optimized IDCT