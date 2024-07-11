#include "LN.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <exception>
#include <fstream>
#include <sstream>

uint8_t from_hex(char ch)
{
	ch = toupper(ch);
	if (ch >= '0' && ch <= '9')
	{
		return ch - '0';
	}
	return ch - 'A' + 10;
}

char to_hex_char(uint8_t value)
{
	if (value >= 0 && value <= 9)
	{
		return '0' + value;
	}
	return 'A' + value - 10;
}

LN::LN(const LN& other)
{
	alloc_(other.size_);
	negative_ = other.negative_;
	nan_ = other.nan_;
	memcpy(data_, other.data_, size_);
}

LN::LN(LN&& other)
{
	data_ = other.data_;
	size_ = other.size_;
	negative_ = other.negative_;
	nan_ = other.nan_;
	other.data_ = nullptr;
	other.size_ = 0;
	other.negative_ = false;
	other.nan_ = false;
}

LN::LN(long long value)
{
	if (value == 0)
	{
		alloc_(1);
		return;
	}
	alloc_(sizeof(value));
	if (value == INT64_MIN)
	{
		negative_ = true;
		data_[0] = from_hex('8');
		for (size_t i = 1; i < size_; ++i)
		{
			data_[i] = 0;
		}
	}
	else if (value < 0)
	{
		negative_ = true;
		value = -value;
	}

	for (size_t i = 0; i < size_; ++i)
	{
		data_[i] = value & MAX_VALUE_;
		value >>= BITS_COUNT_;
	}
	remove_zeros_();
}

LN::LN(const char* str) : LN(std::string_view(str)) {}

void LN::print(std::ofstream& os) const
{
	if (nan_)
	{
		os << "NaN\n";
		return;
	}
	if (is_zero())
	{
		os << "0\n";
		return;
	}
	if (negative_)
	{
		os << "-";
	}
	for (size_t i = size_; i > 0; --i)
	{
		if (!(i == size_ && (data_[i - 1] >> 4) == 0))
		{
			os << to_hex_char(data_[i - 1] >> 4);
		}
		os << to_hex_char(data_[i - 1] & 0xf);
	}
	os << '\n';
}

LN::LN(std::string_view str) : LN()
{
	if (str.empty())
	{
		return;
	}

	bool neg = false;

	if (str[0] == '-')
	{
		neg = true;
		str = str.substr(1);
	}
	if (str.size() >= 2 && str[0] == '0' && toupper(str[1]) == 'X')
	{
		str = str.substr(2);
	}

	size_t byte_count = (str.size() + 1) / 2;
	alloc_(byte_count);
	negative_ = neg;

	size_t shift = 0;

	if (str.size() % 2 == 1)
	{
		data_[byte_count - 1] = from_hex(str[0]);
		shift = 1;
	}
	else
	{
		uint8_t highNibble = from_hex(str[0]);
		uint8_t lowNibble = from_hex(str[1]);
		data_[byte_count - 1] = (highNibble << 4) | lowNibble;
	}
	for (size_t i = 1; i < byte_count; ++i)
	{
		uint8_t highNibble = from_hex(str[i * 2 - shift]);
		uint8_t lowNibble = from_hex(str[i * 2 + 1 - shift]);
		data_[byte_count - i - 1] = (highNibble << 4) | lowNibble;
	}
}

LN operator""_ln(const char* literal)
{
	return LN(literal);
}

LN& LN::operator=(const LN& other)
{
	if (this == &other)
	{
		return *this;
	}

	dealloc_();
	*this = LN(other);
	return *this;
}

LN& LN::operator=(LN&& other)
{
	if (this == &other)
	{
		return *this;
	}
	dealloc_();
	data_ = other.data_;
	size_ = other.size_;
	negative_ = other.negative_;
	nan_ = other.nan_;
	other.data_ = nullptr;
	other.dealloc_();
	return *this;
}

LN LN::operator+(const LN& other) const
{
	if (is_nan() || other.is_nan())
	{
		return create_nan_();
	}
	if (is_negative() && other.is_negative())
	{
		return add_(abs_(*this), abs_(other)).negate();
	}
	else if (is_negative())
	{
		return subtract_(other, abs_(*this));
	}
	else if (other.is_negative())
	{
		return subtract_(*this, abs_(other));
	}
	else
	{
		return add_(*this, other);
	}
}

LN LN::operator-(const LN& other) const
{
	if (is_nan() || other.is_nan())
	{
		return create_nan_();
	}
	if (is_negative() && other.is_negative())
	{
		return subtract_(abs_(other), abs_(*this));
	}
	else if (is_negative())
	{
		return add_(abs_(*this), other).negate();
	}
	else if (other.is_negative())
	{
		return add_(*this, abs_(other));
	}
	else
	{
		return subtract_(*this, other);
	}
}

LN LN::operator*(const LN& other) const
{
	if (is_nan() || other.is_nan())
	{
		return create_nan_();
	}
	if (is_negative() && other.is_negative())
	{
		return multiply_(abs_(*this), abs_(other));
	}
	else if (is_negative() || other.is_negative())
	{
		return multiply_(abs_(*this), abs_(other)).negate();
	}
	else
	{
		return multiply_(*this, other);
	}
}

LN LN::operator/(const LN& other) const
{
	if (is_nan() || other.is_nan())
	{
		return create_nan_();
	}
	if (is_negative() && other.is_negative())
	{
		return divide_(abs_(*this), abs_(other)).first;
	}
	else if (is_negative() || other.is_negative())
	{
		return divide_(abs_(*this), abs_(other)).first.negate();
	}
	else
	{
		return divide_(*this, other).first;
	}
}

LN LN::operator%(const LN& other) const
{
	if (is_nan() || other.is_nan())
	{
		return create_nan_();
	}
	if (is_negative() && other.is_negative())
	{
		return divide_(abs_(*this), abs_(other)).second;
	}
	else if (is_negative() || other.is_negative())
	{
		return divide_(abs_(*this), abs_(other)).second.negate();
	}
	else
	{
		return divide_(*this, other).second;
	}
}

LN LN::operator~() const
{
	if (is_negative() || is_nan())
	{
		return create_nan_();
	}
	if (is_zero())
	{
		return create_zero_();
	}
	LN x = *this;
	LN y = (x + 1) / 2;

	while (y < x)
	{
		x = y;
		y = (x + *this / x) / 2;
	}

	return x;
}

LN LN::operator-() const
{
	return negate();
}

LN& LN::operator+=(const LN& other)
{
	*this = *this + other;
	return *this;
}

LN& LN::operator-=(const LN& other)
{
	*this = *this - other;
	return *this;
}

LN& LN::operator*=(const LN& other)
{
	*this = *this * other;
	return *this;
}

LN& LN::operator/=(const LN& other)
{
	*this = *this / other;
	return *this;
}

LN& LN::operator%=(const LN& other)
{
	*this = *this % other;
	return *this;
}

bool LN::operator<(const LN& other) const
{
	if (is_nan() || other.is_nan())
	{
		return false;
	}
	return compare_(*this, other) < 0;
}

bool LN::operator<=(const LN& other) const
{
	if (is_nan() || other.is_nan())
	{
		return false;
	}
	return compare_(*this, other) <= 0;
}

bool LN::operator>(const LN& other) const
{
	if (is_nan() || other.is_nan())
	{
		return false;
	}
	return compare_(*this, other) > 0;
}

bool LN::operator>=(const LN& other) const
{
	if (is_nan() || other.is_nan())
	{
		return false;
	}
	return compare_(*this, other) >= 0;
}

bool LN::operator==(const LN& other) const
{
	if (is_nan() || other.is_nan())
	{
		return false;
	}
	return compare_(*this, other) == 0;
}

bool LN::operator!=(const LN& other) const
{
	if (is_nan() || other.is_nan())
	{
		return false;
	}
	return compare_(*this, other) != 0;
}

LN::operator long long() const
{
	if (is_nan())
	{
		throw std::overflow_error("Conversion to long long failed. Value is NaN.");
	}

	if (size_ > sizeof(long long))
	{
		throw std::overflow_error("Conversion to long long failed. Value too large.");
	}

	uint64_t value = 0;
	for (size_t i = 0; i < size_; ++i)
	{
		value |= static_cast< uint64_t >(data_[i]) << (i * 8);
	}

	if (negative_)
	{
		value = -value;
	}

	return value;
}

LN::operator bool() const
{
	return !is_zero();
}

bool LN::is_negative() const
{
	return negative_;
}

bool LN::is_zero() const
{
	if (is_nan())
	{
		return false;
	}
	if (size_ == 0)
	{
		return true;
	}

	for (size_t i = 0; i < size_; ++i)
	{
		if (data_[i] != 0)
		{
			return false;
		}
	}

	return true;
}

bool LN::is_nan() const
{
	return nan_;
}

size_t LN::size() const
{
	return size_;
}

LN LN::add_(const LN& lhs, const LN& rhs)
{
	if (lhs.size_ < rhs.size_)
	{
		return add_(rhs, lhs);
	}

	LN result;
	result.size_ = lhs.size_ + 1;
	result.alloc_(result.size_ + 1);
	memset(result.data_, 0, result.size_);

	uint16_t carry = 0;
	for (size_t i = 0; i < lhs.size_; ++i)
	{
		uint16_t sum = static_cast< uint16_t >(lhs.data_[i]) + carry;
		if (i < rhs.size_)
		{
			sum += static_cast< uint16_t >(rhs.data_[i]);
		}

		result.data_[i] = static_cast< uint8_t >(sum);
		carry = sum >> BITS_COUNT_;
	}

	result.data_[lhs.size_] = static_cast< uint8_t >(carry);

	result.remove_zeros_();

	return result;
}

LN LN::subtract_(const LN& lhs, const LN& rhs)
{
	if (lhs < rhs)
	{
		return subtract_(rhs, lhs).negate();
	}

	LN result;
	result.size_ = lhs.size_;
	result.alloc_(result.size_);
	memset(result.data_, 0, result.size_);

	uint16_t borrow = 0;
	for (size_t i = 0; i < lhs.size_; ++i)
	{
		uint16_t diff = static_cast< uint16_t >(lhs.data_[i]) - borrow;
		if (i < rhs.size_)
		{
			diff -= static_cast< uint16_t >(rhs.data_[i]);
		}

		if (diff > LN::MAX_VALUE_)
		{
			borrow = 1;
			diff += LN::MAX_VALUE_ + 1;
		}
		else
		{
			borrow = 0;
		}

		result.data_[i] = static_cast< uint8_t >(diff);
	}

	result.remove_zeros_();

	return result;
}

LN LN::multiply_(const LN& lhs, const LN& rhs)
{
	if (lhs.is_zero() || rhs.is_zero())
	{
		return create_zero_();
	}
	if (std::min(lhs.size(), rhs.size()) <= size_for_fast_)
	{
		return multiply_slow_(lhs, rhs);
	}
	LN temp_lhs = lhs;
	LN temp_rhs = rhs;
	if (lhs.size() < rhs.size())
	{
		temp_lhs = pad_(lhs, rhs.size());
	}
	if (rhs.size() < lhs.size())
	{
		temp_rhs = pad_(rhs, lhs.size());
	}
	size_t n = std::min(temp_lhs.size(), temp_rhs.size()) / 2;
	LN a, b, c, d;
	b.alloc_(n);
	memcpy(b.data_, temp_lhs.data_ + temp_lhs.size() - n, n);
	d.alloc_(n);
	memcpy(d.data_, temp_rhs.data_ + temp_rhs.size() - n, n);

	a.alloc_(temp_lhs.size() - n);
	memcpy(a.data_, temp_lhs.data_, temp_lhs.size() - n);
	c.alloc_(temp_rhs.size() - n);
	memcpy(c.data_, temp_rhs.data_, temp_rhs.size() - n);

	n = temp_lhs.size() - n;

	LN ac, bd, abcd;
	ac = a * c;
	bd = b * d;
	abcd = (a + b) * (c + d);
	return shift_left_(bd, n + n) + shift_left_(abcd - ac - bd, n) + ac;
}

LN LN::multiply_slow_(const LN& lhs, const LN& rhs)
{
	if (lhs.is_zero() || rhs.is_zero())
	{
		return create_zero_();
	}
	LN result;
	result.size_ = lhs.size_ + rhs.size_;
	result.alloc_(result.size_);
	memset(result.data_, 0, result.size_);

	for (size_t i = 0; i < lhs.size_; ++i)
	{
		uint8_t digitA = lhs.data_[i];
		uint8_t carry = 0;

		for (size_t j = 0; j < rhs.size_; ++j)
		{
			uint8_t digitB = rhs.data_[j];
			uint16_t product = static_cast< uint16_t >(digitA) * digitB + carry + result.data_[i + j];
			result.data_[i + j] = static_cast< uint8_t >(product);
			carry = static_cast< uint8_t >(product >> LN::BITS_COUNT_);
		}

		if (carry > 0)
		{
			result.data_[i + rhs.size_] = carry;
		}
	}

	result.remove_zeros_();
	return result;
}

// since pair has a trivial destructor
std::pair< LN, LN > LN::divide_(const LN& dividend, const LN& divisor)
{
	if (divisor.is_zero())
	{
		return { create_nan_(), create_nan_() };
	}

	if (dividend.size_ < divisor.size_)
	{
		return { create_zero_(), dividend };
	}

	LN quotient;
	quotient.size_ = dividend.size_ - divisor.size_ + 1;
	quotient.alloc_(quotient.size_);
	memset(quotient.data_, 0, quotient.size_);

	LN remainder = dividend;

	while (remainder >= divisor)
	{
		size_t shift = remainder.size_ - divisor.size_;
		LN multiple = shift_left_(divisor, shift);

		if (remainder < multiple)
		{
			multiple = shift_left_(divisor, --shift);
		}

		uint8_t digit = 0;
		while (remainder >= multiple)
		{
			remainder -= multiple;
			digit++;
		}

		quotient.data_[shift] = digit;
	}
	quotient.remove_zeros_();
	remainder.remove_zeros_();
	return { quotient, remainder };
}

int LN::compare_(const LN& lhs, const LN& rhs)
{
	int ret = -1;	 // Return in case digit_a < digit_b;
	if (lhs.is_negative() && !rhs.is_negative())
	{
		return -1;
	}
	if (!lhs.is_negative() && rhs.is_negative())
	{
		return 1;
	}
	if (lhs.is_negative() && rhs.is_negative())
	{
		ret = 1;
	}
	if (lhs.size_ < rhs.size_)
	{
		return -1;
	}
	else if (lhs.size_ > rhs.size_)
	{
		return 1;
	}

	for (size_t i = lhs.size_; i > 0; --i)
	{
		uint8_t digitA = lhs.data_[i - 1];
		uint8_t digitB = rhs.data_[i - 1];

		if (digitA < digitB)
		{
			return ret;
		}
		else if (digitA > digitB)
		{
			return -ret;
		}
	}

	return 0;
}

LN LN::shift_left_(const LN& number, size_t shift)
{
	LN shifted;
	shifted.size_ = number.size_ + shift;
	shifted.alloc_(shifted.size_);

	memcpy(shifted.data_ + shift, number.data_, number.size_);

	return shifted;
}

LN LN::pad_(const LN& number, size_t size)
{
	LN result;
	result.alloc_(size);
	memcpy(result.data_, number.data_, number.size());
	return result;
}

LN LN::create_zero_()
{
	LN zero;
	return zero;
}

LN LN::create_nan_()
{
	LN nan;
	nan.nan_ = true;
	return nan;
}

void LN::dealloc_()
{
	delete[] data_;
	data_ = nullptr;
	size_ = 0;
	nan_ = false;
	negative_ = false;
}

void LN::alloc_(size_t size)
{
	dealloc_();
	data_ = new uint8_t[size];
	memset(data_, 0, size);
	size_ = size;
	negative_ = false;
	nan_ = false;
}

LN LN::abs_(const LN& obj)
{
	LN result = obj;
	result.negative_ = false;
	return result;
}

void LN::remove_zeros_()
{
	while (size_ > 0 && data_[size_ - 1] == 0)
	{
		size_--;
	}
	if (size_ == 0)
	{
		negative_ = false;
	}
}

LN LN::negate() const
{
	LN result = *this;
	result.negative_ = !result.negative_;
	return result;
}

LN::~LN()
{
	dealloc_();
}