#include "JPGDecoder.hpp"
#include <stdexcept>
#include <iostream>

int main() {
	try {
		auto contents = JPG::JPGDecoder::ReadJPG("plswork.jpg");

		std::unique_ptr<JPG::MCU[]> mcus = JPG::JPGDecoder::DecodeJPG(*contents);

		JPG::JPGDecoder::WriteBMPFromJPG(*contents, mcus.get(), "test.bmp");
	}
	catch (const std::logic_error& e) {
		std::cout << "Logic Error - " << e.what() << '\n';
	}
	catch (const std::exception& e) {
		std::cout << "Error - " << e.what() << '\n';
	}
	catch (...) {
		std::cout << "Error - Unknown Error\n";
	}
	return 0;
}