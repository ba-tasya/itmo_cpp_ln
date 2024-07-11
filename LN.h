#pragma once
#include <cstdlib>
#include <iostream>
#include <string>

class LN
{
  public:
	// Constructors
	LN(long long value = 0);
	LN(const char* value);
	LN(std::string_view value);
	LN(const LN& other);
	LN(LN&& other);

	// Operators
	LN& operator=(const LN& other);
	LN& operator=(LN&& other);

	LN operator+(const LN& other) const;
	LN operator-(const LN& other) const;
	LN operator/(const LN& other) const;
	LN operator*(const LN& other) const;
	LN operator%(const LN& other) const;
	LN operator~() const;
	LN operator-() const;

	LN& operator+=(const LN& other);
	LN& operator-=(const LN& other);
	LN& operator/=(const LN& other);
	LN& operator*=(const LN& other);
	LN& operator%=(const LN& other);

	bool operator<(const LN& other) const;
	bool operator<=(const LN& other) const;
	bool operator>(const LN& other) const;
	bool operator>=(const LN& other) const;
	bool operator==(const LN& other) const;
	bool operator!=(const LN& other) const;

	explicit operator long long() const;
	explicit operator bool() const;

	bool is_negative() const;
	bool is_zero() const;
	bool is_nan() const;
	LN negate() const;
	void print(std::ofstream& os) const;

	~LN();

	size_t size() const;

  private:
	void alloc_(size_t size);
	void dealloc_();
	void remove_zeros_();

	uint8_t* data_ = nullptr;
	size_t size_ = 0;
	bool negative_ = false;
	bool nan_ = false;

	static int compare_(const LN& lhs, const LN& rhs);

	static LN add_(const LN& lhs, const LN& rhs);
	static LN subtract_(const LN& lhs, const LN& rhs);
	static LN multiply_slow_(const LN& lhs, const LN& rhs);
	static LN multiply_(const LN& lhs, const LN& rhs);
	static std::pair< LN, LN > divide_(const LN& lhs, const LN& rhs);
	static LN abs_(const LN& obj);
	static LN create_nan_();
	static LN create_zero_();

	static LN shift_left_(const LN& obj, size_t shift_size);
	static LN pad_(const LN& obj, size_t size);

	static const size_t BITS_COUNT_ = 8;
	static const uint16_t MAX_VALUE_ = 0xFF;
	static const size_t size_for_fast_ = 256;
};