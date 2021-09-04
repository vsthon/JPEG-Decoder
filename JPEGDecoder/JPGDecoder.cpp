#include "JPGDecoder.hpp"
#include <iostream>
#define NOMINMAX
#include <Windows.h>
#include <algorithm>
#include <cmath>

namespace JPG {



	// --- Read JPG Functions --- \\



	std::unique_ptr<JPGFile> JPGDecoder::ReadJPG(const std::string & filename) {
		// open file
		std::ifstream file(filename, std::ios::in | std::ios::binary);

		if (!file.is_open()) {
			throw std::invalid_argument("Cannot open file! (Is the filename correct?)");
		}

		std::unique_ptr<JPGFile> jpgContents = std::make_unique<JPGFile>();

		byte markerFF = file.get();
		byte markerID = file.get();

		if (markerFF != 0xFF || markerID != SOI) {
			file.close();
			throw std::invalid_argument("Invalid JPG file (markerFF is not FF or markerID is not SOI at the beginnning)");
		}
		markerFF = file.get();
		markerID = file.get();

		while (true) {

			if (markerFF != 0xFF) {
				throw std::invalid_argument("Invalid JPG file (markerFF is not 0xFF)");
			}
			if (!file) {
				throw std::invalid_argument("Invalid JPG File (File ended without reaching EOF marker)");
			}

			if (markerID >= APP0 && markerID <= APP15) {
				ProcessAPPN(file, *jpgContents);
			}
			else if (markerID == COM) {
				ProcessComment(file, *jpgContents);
			}
			else if (markerID == DRI) {
				ProcessRestartInterval(file, *jpgContents);
			}
			else if (markerID == DQT) {
				ProcessQuantizationTable(file, *jpgContents);
			}
			else if (markerID == DHT) {
				ProcessHuffmanTable(file, *jpgContents);
			}
			else if (markerID == SOF0) {
				ProcessStartOfFrame(file, *jpgContents);
			}
			else if (markerID == SOS) {
				ProccesStartOfScan(file, *jpgContents);
				break;
			}
			else if (markerID == 0xFF) {
				markerID = file.get();
				continue;
			}

			// TODO: Add handlers for unused / error markers.
			markerFF = file.get();
			markerID = file.get();
		}

		markerID = file.get();

		// read huffman coded bitstream
		while (true) {
			if (!file) {
				throw std::invalid_argument("Invalid JPG File (File ended without reaching EOF marker in HCB)");
			}

			markerFF = markerID;
			markerID = file.get();

			if (markerFF == 0xFF) {
				if (markerID == EOI) {
					break;
				}
				else if (markerID == 0x00) {
					jpgContents->huffmanBitstream.push_back(markerFF);
					markerID = file.get();
				}
				else if (markerID >= RST0 && markerID <= RST7) {
					markerID = file.get();
				}
				else if (markerID == 0xFF) {
					continue;
				}
				else {
					throw std::invalid_argument("Invalid JPG File (Unknown marker at HCB)");
				}
			}
			else {
				jpgContents->huffmanBitstream.push_back(markerFF);
			}
		}

		if (jpgContents->numComponents != 1 && jpgContents->numComponents != 3) {
			throw std::length_error("Invalid JPG (Unsupported NumComponents)");
		}

		for (uint i = 0; i < jpgContents->numComponents; i++) {
			if (jpgContents->qtTables[jpgContents->components[i].quantizationTableID].set == false) {
				throw std::invalid_argument("Invalid JPG (component uses an uninitialized qTable)");
			}
			if (jpgContents->huffmanACTables[jpgContents->components[i].huffmanACTableID].set == false) {
				throw std::invalid_argument("Invalid JPG (component uses an uninitialized AC Table)");
			}
			if (jpgContents->huffmanDCTables[jpgContents->components[i].huffmanDCTableID].set == false) {
				throw std::invalid_argument("Invalid JPG (component uses an uninitialized DC Table)");
			}
		}

		return std::move(jpgContents);
	}
	void JPGDecoder::WriteBMPFromJPG(const JPGFile& contents, const MCU mcus[], const std::string& fileName) {
		auto writeInt = [](std::ofstream& out, int theint) {
			out.put((theint >> 0) & 0xFF);
			out.put((theint >> 8) & 0xFF);
			out.put((theint >> 16) & 0xFF);
			out.put((theint >> 24) & 0xFF);
		};
		auto writeShort = [](std::ofstream& out, uint16_t theShort) {
			out.put((theShort >> 0) & 0xFF);
			out.put((theShort >> 8) & 0xFF);
		};
		std::ofstream out(fileName, std::ios::out | std::ios::binary);
		const uint padding = (contents.width % 4) * contents.height;
		out.put('B');
		out.put('M');
		writeInt(out, 14 + 12 + ((contents.width * contents.height) * contents.numComponents) + padding);
		writeInt(out, 0);
		writeInt(out, 0x1A);
		writeInt(out, 12);
		writeShort(out, contents.width);
		writeShort(out, contents.height);
		writeShort(out, 1);
		writeShort(out, 24);

		for (int y = contents.height - 1; y >= 0; y--) {
			for (uint x = 0; x < contents.width; x++) {
				const MCU& mcu = mcus[(y / 8) * contents.mcuWidth + (x / 8)];
				uint mcuX = x % 8;
				uint mcuY = y % 8;
				out.put(std::min(std::max(mcu.cr[mcuY * 8 + mcuX], 0), 255));
				out.put(std::min(std::max(mcu.cb[mcuY * 8 + mcuX], 0), 255));
				out.put(std::min(std::max(mcu.y[mcuY * 8 + mcuX], 0), 255));
			}
			for (uint i = 0; i < padding; i++) {
				out.put(0);
			}
		}
	}
	// APP(N) Marker
	void JPGDecoder::ProcessAPPN(std::ifstream& file, JPGFile& jpgContents) {
		std::cout << "Reading APPN marker\n";
		uint length = (file.get() << 8) + file.get();
	
		for (uint i = 0; i < length - 2; i++) {
			file.get();
		}
	}
	// Quantization Tables
	void JPGDecoder::ProcessQuantizationTable(std::ifstream& file, JPGFile& jpgContents) {
		std::cout << "Reading DQT marker\n";
		int length = (file.get() << 8) + file.get();
		length -= 2;

		while(length > 0) {
			byte tableInfo = file.get();
			length--;
			byte tableID = tableInfo & 0x0F;

			if (tableID < 0 || tableID > 3) {
				throw std::invalid_argument("Invalid JPG (table ID for DQT is invalid).");
			}

			jpgContents.qtTables[tableID].set = true;

			if ((tableInfo >> 4) != 0) {
				for (uint i = 0; i < 64; i++) {
					jpgContents.qtTables[tableID].table[MCUMap[i]] = (file.get() << 8) + file.get();
				}
				length -= 128;
			}
			else {
				for (uint i = 0; i < 64; i++) {
					jpgContents.qtTables[tableID].table[MCUMap[i]] = file.get();
				}
				length -= 64;
			}
		}

		if (length != 0) {
			throw std::length_error("Invalid DQT Marker (length is invalid)");
		}
	}
	// Start of scan (for hcb)
	void JPGDecoder::ProccesStartOfScan(std::ifstream& file, JPGFile& jpgContents) {
		std::cout << "Reading SOS Marker\n";
		if (jpgContents.numComponents == 0) {
			throw std::invalid_argument("Invalid SOS Marker (read SOF before SOS which is not allowed)");
		}

		uint length = (file.get() << 8) + file.get();
		
		for (uint i = 0; i < jpgContents.numComponents; i++) {
			jpgContents.components[i].used = false;
		}

		byte numComponents = file.get();
		
		for (uint i = 0; i < numComponents; i++) {
			byte componentID = file.get();
			if (jpgContents.zerobased) {
				componentID++;
			}
			if (componentID > numComponents) {
				throw std::length_error("Invalid SOS Marker (component ID is invalid)");
			}

			JPG::ColorComponent& component = jpgContents.components[componentID - 1];

			if (component.used) {
				throw std::invalid_argument("Invalid SOS Marker (reading same component ID twice)");
			}
			component.used = true;

			byte huffmanTableIDs = file.get();
			component.huffmanDCTableID = huffmanTableIDs >> 4;
			component.huffmanACTableID = huffmanTableIDs & 0x0F;
			if (component.huffmanACTableID > 3 || component.huffmanDCTableID > 3) {
				throw std::length_error("Invalid SOS Marker (one of a component's huffman table ID is invalid)");
			}
		}

		jpgContents.startOfSelection = file.get();
		jpgContents.endOfSelection = file.get();
		byte successiveApproximation = file.get();
		jpgContents.successiveApproximationHigh = successiveApproximation >> 4;
		jpgContents.successiveApproximationLow = successiveApproximation & 0x0F;

		if (jpgContents.startOfSelection != 0 || jpgContents.endOfSelection != 63) {
			throw std::length_error("Invalid SOS Marker (start of selection is invalid)");
		}
		if (jpgContents.successiveApproximationHigh != 0 || jpgContents.successiveApproximationLow != 0) {
			throw std::length_error("Invalid SOS Marker (successive approximation is invalid)");
		}

		if (length - 6 - (2 * numComponents) != 0) {
			throw std::length_error("Invalid SOS (length is not equal to 0 after reading marker)");
		}
	}
	void JPGDecoder::ProcessComment(std::ifstream& file, JPGFile& jpgContents) {
		std::cout << "Reading COM Marker\n";
		uint length = (file.get() << 8) + file.get();
		
		for (uint i = 0; i < length - 2; i++) {
			file.get();
		}
	}
	// Huffman Tables
	void JPGDecoder::ProcessHuffmanTable(std::ifstream& file, JPGFile& jpgContents) {
		std::cout << "Reading DHT Marker\n";
		int length = (file.get() << 8) + file.get();
		length -= 2;

		while (length > 0) {
			byte tableInfo = file.get();
			byte tableID = tableInfo & 0x0F;
			bool ACTable = tableInfo >> 4;

			if (tableID < 0 || tableID > 3) {
				throw std::invalid_argument("Invalid DHT Marker (table ID for DHT is invalid).");
			}

			HuffmanTable& hTable = ACTable ? jpgContents.huffmanACTables[tableID] : jpgContents.huffmanDCTables[tableID];
			hTable.set = true;

			uint numSymbols = 0;
			byte numCodesOfLength[16];
			for (uint i = 0; i < 16; i++) {
				byte numCodesOfLengthI = file.get();
				numSymbols += numCodesOfLengthI;
				numCodesOfLength[i] = numCodesOfLengthI;
			}
			if (numSymbols > 162) {
				throw std::length_error("Invalid DHT Marker (too many symbols)");
			}

			for (uint i = 0; i < 16; i++) {
				hTable.symbols[i].resize(numCodesOfLength[i]);
				for (uint j = 0; j < numCodesOfLength[i]; j++) {
					hTable.symbols[i][j] = file.get();
				}
			}

			length -= 17 + numSymbols;
		}
		if (length != 0) {
			throw std::length_error("Invalid DHT Marker (length is not 0)");
		}
	}
	// SOF Marker
	void JPGDecoder::ProcessStartOfFrame(std::ifstream& file, JPGFile& jpgContents) {
		std::cout << "Reading SOF (Start of Frame)\n";

		if (jpgContents.numComponents != 0) {
			throw std::invalid_argument("Invalid SOF Marker (there are more than one SOF marker which is not allowed)");
		}

		uint length = (file.get() << 8) + file.get();

		byte precision = file.get();
		if (precision != 8) {
			throw std::length_error("Invalid SOF Marker (precision is invalid)");
		}

		jpgContents.height = (file.get() << 8) + file.get();
		jpgContents.mcuHeight = (jpgContents.height + 7) / 8;
		jpgContents.width = (file.get() << 8) + file.get();
		jpgContents.mcuWidth = (jpgContents.width + 7) / 8;

		if (jpgContents.height == 0 || jpgContents.width == 0) {
			throw std::length_error("Invalid JPG (width or height are 0)");
		}

		jpgContents.numComponents = file.get();
		if (jpgContents.numComponents == 4) {
			throw std::invalid_argument("Unsupported JPG (CMYK colors are not supported)");
		}
		if (jpgContents.numComponents == 0) {
			throw std::length_error("Invalid SOF Marker (numComponents is 0)");
		}

		for (uint i = 0; i < jpgContents.numComponents; i++) {
			byte componentID = file.get();

			if (componentID == 0) {
				jpgContents.zerobased = true;
			}
			if (jpgContents.zerobased) {
				componentID++;
			}

			if(componentID == 4 || componentID == 5) {
				throw std::invalid_argument("Unsupported JPG (YIQ colors are not supported)");
			}
			if (componentID == 0 || componentID > 3) {
				throw std::invalid_argument("Invalid JPG (componentID is invalid)");
			}
			ColorComponent& component = jpgContents.components[componentID - 1];
			if (component.used == true) {
				throw std::invalid_argument("Invalid SOF Marker (componentID showed up more than once)");
			}
			component.used = true;
			byte samplingFactor = file.get();
			component.HSF = samplingFactor >> 4;
			component.VSF = samplingFactor & 0x0F;
			//if (component.HSF != 1 || component.VSF != 1)
			//{
			//	throw std::invalid_argument("Unsupported JPG (SF 1 is only supported for the time being)");
			//}
			
			component.quantizationTableID = file.get();
			if (component.quantizationTableID > 3) {
				throw std::length_error("Invalid SOF Marker (QTID is greater than 3 for some reason)");
			}
		}
		if (length - 8 - (3 * jpgContents.numComponents) != 0) {
			throw std::invalid_argument("Invalid SOF Marker (length is not equal to 0)");
		}
	}
	// DRI Marker
	void JPGDecoder::ProcessRestartInterval(std::ifstream& file, JPGFile& jpgContents) {
		std::cout << "Reading DRI Marker\n";
		uint length = (file.get() << 8) + file.get();
		jpgContents.restartInterval = (file.get() << 8) + file.get();
		
		if (length - 4 != 0) {
			throw std::length_error("Invalid DRI Marker (length is invalid)");
		}
	}



	// --- Decode JPG Functions --- \\

	
	class BitReader {
		const std::vector<byte>& bytes;
		uint nextByte = 0;
		uint nextBit = 0;
	public:
		BitReader(const std::vector<byte>& bytes) :
			bytes(bytes)
		{}

		int readBit() {
			if (nextByte >= bytes.size()) {
				return -1;
			}
			int bit = (bytes[nextByte] >> (7 - nextBit)) & 1;
			nextBit++;
			if (nextBit == 8) {
				nextBit = 0;
				nextByte += 1;
			}
			return bit;
		}

		int readBits(const uint length) {
			int bits = 0;
			for (uint i = 0; i < length; i++) {
				int bit = readBit();
				if (bit == -1) {
					bits = -1;
					break;
				}
				bits = (bits << 1) | bit;
			}
			return bits;
		}
	};


	std::unique_ptr<MCU[]> JPGDecoder::DecodeJPG(JPGFile& contents) {
		auto mcus = std::make_unique<MCU[]>(contents.mcuWidth * contents.mcuHeight);
		DecodeHuffmanData(mcus.get(), contents);
		DequantizeMCUs(mcus.get(), contents);
		InverseDiscreteCosineTransform(mcus.get(), contents);
		YCbCrToRGB(mcus.get(), contents);
		return std::move(mcus);
	}

	std::string convert(int num, int size) {
		std::string theThing;
		for (int i = 31; i >= 0; i--) {
			int k = num >> i;
			if (k & 1) {
				if(i < size)
					theThing.push_back('1');
			}
			else {
				if (i < size)
					theThing.push_back('0');
			}
		}
		theThing.push_back('\n');
		return theThing;
	}

	void JPGDecoder::GenerateHuffmanCodes(HuffmanTable& hTable) {
		uint code = 0;
		for (uint i = 0; i < 16; i++) {
			for (uint j = 0; j < hTable.symbols[i].size(); j++) {
				hTable.codes[i].push_back(code);
				OutputDebugStringA(convert(code, i + 1).c_str());
				code += 1;
			}
			code <<= 1;
		}
	}

	void JPGDecoder::DecodeHuffmanData(MCU mcus[], JPGFile& contents) {
		// Generate codes for all the used huffman tables
		for (uint i = 0; i < 4; ++i) {
			if (contents.huffmanACTables[i].set) {
				GenerateHuffmanCodes(contents.huffmanACTables[i]);
			}
			if (contents.huffmanDCTables[i].set) {
				GenerateHuffmanCodes(contents.huffmanDCTables[i]);
			}
		}

		BitReader b(contents.huffmanBitstream);
		int prevCoeff[3] = { 0 };

		for (uint i = 0; i < contents.mcuWidth * contents.mcuHeight; i++) {
			for (uint j = 0; j < contents.numComponents; j++) {
				DecodeMCUComponent(b, 
					contents.huffmanDCTables[contents.components[j].huffmanDCTableID], 
					contents.huffmanACTables[contents.components[j].huffmanACTableID], 
					mcus[i][j], prevCoeff[j]);
			}
		}
	}
	byte GetNextSymbol(BitReader& b, const HuffmanTable& huffmanTable) {
		int code = 0;
		for (uint i = 0; i < 16; i++) {
			int bit = b.readBit();
			if (bit == -1) {
				return -1;
			}
			code = (code << 1) | bit;
			for (uint j = 0; j < huffmanTable.codes[i].size(); j++) {
				if (code == huffmanTable.codes[i][j]) {
					return huffmanTable.symbols[i][j];
				}
			}
		}
		return -1;
	}
	void JPGDecoder::DecodeMCUComponent(BitReader& b, HuffmanTable& huffmanDCTable, HuffmanTable& huffmanACTable, int MCUComponent[64], int& prevCoeff) {
		// Read the DC symbol for this mcu component
		byte length = GetNextSymbol(b, huffmanDCTable);
		if (length == (byte)-1) {
			throw std::invalid_argument("Something went wrong when trying to read a symbol (DC 1)");
		}
		if (length > 11) {
			throw std::length_error("DC lengths can not be longer than 11");
		}
		int coeff = b.readBits(length);
		if (coeff == -1) {
			throw std::invalid_argument("Something went wrong when trying to read a symbol (DC 2)");
		}
		if (length != 0 && coeff < (1 << (length - 1))) {
			coeff -= (1 << length) - 1;
		}
		MCUComponent[0] = coeff + prevCoeff;
		prevCoeff = MCUComponent[0];
		
		// Read the AC symbols for this MCU component
		uint i = 1;
		while (i < 64) {
			byte symbol = GetNextSymbol(b, huffmanACTable);
			if (symbol == (byte)-1) {
				throw std::invalid_argument("Something went wrong when trying to read a symbol (AC 1)");
			}
			if (symbol == 0x00) {
				for (; i < 64; ++i) {
					MCUComponent[MCUMap[i]] = 0;
				}
				return;
			}

			byte numZeros = symbol >> 4;
			byte coeffLength = symbol & 0x0F;
			coeff = 0;
			if (coeffLength > 10) {
				throw std::length_error("AC lengths can not be longer than 10");
			}
			if (symbol == 0xF0) {
				numZeros = 16;
			}

			for (uint j = 0; j < numZeros; ++j, ++i) {
				MCUComponent[MCUMap[i]] = 0;
			}

			if (coeffLength != 0) {
				coeff = b.readBits(coeffLength);
				if (coeff == -1) {
					throw std::invalid_argument("Something went wrong when trying to read a symbol (AC 2)");
				}
				if (coeff < (1 << (coeffLength - 1))) {
					coeff -= (1 << coeffLength) - 1;
				}
				MCUComponent[MCUMap[i]] = coeff;
				i += 1;
			}
		}
	}
	void JPGDecoder::DequantizeMCUs(MCU mcus[], JPGFile& contents) {
		for (uint i = 0; i < contents.mcuWidth * contents.mcuHeight; i++) {
			for (uint j = 0; j < contents.numComponents; j++) {
				for (uint k = 0; k < 64; k++) {
					mcus[i][j][k] *= contents.qtTables[contents.components[j].quantizationTableID].table[k];
				}
			}
		}
	}
	void JPGDecoder::InverseDiscreteCosineTransform(MCU MCUComponent[], JPGFile& contents) {
		for (uint k = 0; k < contents.mcuWidth * contents.mcuHeight; k++) {
			for (uint l = 0; l < contents.numComponents; l++) {
				int result[64] = { 0 };
				for (uint x = 0; x < 8; x++) {
					for (uint y = 0; y < 8; y++) {
						float sum = 0;
						for (uint i = 0; i < 8; i++) {
							for (uint j = 0; j < 8; j++) {
								const float ci = i == 0 ? (1.f / std::sqrt(2.f)) : 1;
								const float cj = j == 0 ? (1.f / std::sqrt(2.f)) : 1;
								const float idct = ci * cj * MCUComponent[k][l][i * 8 + j] *
									(float)std::cos((((2.0 * y) + 1.0) * 3.14159265 * i) / 16.0) *
									(float)std::cos((((2.0 * x) + 1.0) * 3.14159265 * j) / 16.0);
								sum += idct;
							}
						}
						sum /= 4;
						result[y * 8 + x] = (int)sum;
					}
				}
				for (uint x = 0; x < 8; x++) {
					for (uint y = 0; y < 8; y++) {
						MCUComponent[k][l][y * 8 + x] = result[y * 8 + x];
					}
				}
			}
		}
	}
	void JPGDecoder::YCbCrToRGB(MCU mcus[], JPGFile& contents) {
		for (uint i = 0; i < contents.mcuWidth * contents.mcuHeight; i++) {
			for (uint x = 0; x < 8; x++) {
				for (uint y = 0; y < 8; y++) {
					int r = int(mcus[i].y[y * 8 + x] + 1.402f * (mcus[i].cr[y * 8 + x])) + 128;
					int g = int(mcus[i].y[y * 8 + x] - 0.344136f * (mcus[i].cb[y * 8 + x]) - 0.714136f * (mcus[i].cr[y * 8 + x])) + 128;
					int b = int(mcus[i].y[y * 8 + x] + 1.772f * (mcus[i].cb[y * 8 + x])) + 128;

					mcus[i].y[y * 8 + x] = std::min(std::max(r, 255), 255);
					mcus[i].cb[y * 8 + x] = std::min(std::max(g, 255), 255);
					mcus[i].cr[y * 8 + x] = std::min(std::max(b, 255), 255);
				}
			}
		}
	}
}