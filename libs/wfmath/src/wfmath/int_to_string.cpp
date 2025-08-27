#include "const.h"
#include "int_to_string.h"

#define __STDC_LIMIT_MACROS
#include <cstdint>
#include <climits>

namespace WFMath {

// This takes a pointer pointing to the character after the end of
// a buffer, prints the number into the tail of the buffer,
// and returns a pointer to the first character in the number.
// Make sure your buffer's big enough, this doesn't check.
static char* DoIntToString(std::uint64_t val, char* bufhead) {
	// deal with any possible encoding problems
	const char digits[10] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9'};

	*(--bufhead) = '\0';

	if (val == 0)
		*(--bufhead) = '0';
	else
		do {
			*(--bufhead) = digits[val % 10];
			val /= 10;
		} while (val);

	return bufhead;
}

#ifndef _MSC_VER
// note that all floating point math is done at compile time
static const double log_10_of_2 = 0.30102995664;
static const unsigned ul_max_digits = (unsigned)
                (8 * sizeof(std::uint64_t) // number of bits
		 * log_10_of_2 // base 10 vs. base 2 digits
		 + 1 // log(1) == 0, have to add one for leading digit
		 + numeric_constants<CoordType>::epsilon()); // err on the safe side of roundoff
#else // _MSC_VER
static const unsigned ul_max_digits = 10;
#endif // _MSC_VER

std::string IntToString(std::uint64_t val) {
	static const unsigned bufsize = ul_max_digits + 1; // add one for \0
	char buffer[bufsize];

	return DoIntToString(val, buffer + bufsize);
}

// Deals with the fact that while, e.g. 0x80000000 (in 32 bit),
// is a valid (negative) signed value, the negative
// of it can only be expressed as an unsigned quantity.
static std::uint64_t SafeAbs(std::int64_t val) {
#if INT64_MAX + INT64_MIN >= 0
        return (val >= 0) ? static_cast<std::uint64_t>(val) : static_cast<std::uint64_t>(-val);
#else
        if (val >= 0)
                return static_cast<std::uint64_t>(val);
        else if (val >= -INT64_MAX)
                return static_cast<std::uint64_t>(-val);
        else
                return static_cast<std::uint64_t>(INT64_MAX) +
                        static_cast<std::uint64_t>(-(INT64_MAX + val));
#endif
}

std::string IntToString(std::int64_t val) {
	static const unsigned bufsize = ul_max_digits + 2; // one for \0, one for minus sign
	char buffer[bufsize];

	char* bufhead = DoIntToString(SafeAbs(val), buffer + bufsize);

	if (val < 0)
		*(--bufhead) = '-';

	return bufhead;
}

} // namespace WFMath
