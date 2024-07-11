#include "LN.h"
#include "return_codes.h"

#include <cstdint>
#include <fstream>
#include <iostream>
#include <new>
#include <ostream>
#include <vector>

void add(std::vector< LN >& stack, const LN& obj)
{
	stack.push_back(obj);
}

void two_previous(std::vector< LN >& stack, LN& a, LN& b)
{
	a = stack.back();
	stack.pop_back();
	b = stack.back();
	stack.pop_back();
}

void one_previous(std::vector< LN >& stack, LN& a)
{
	a = stack.back();
	stack.pop_back();
}

int main(int argc, const char* argv[])
{
	int result = SUCCESS;
	std::ifstream is;
	std::ofstream os;
	std::string s;
	std::vector< LN > stack;
	if (argc != 3)
	{
		std::cerr << "Expected two arguments: input file name and output file name.";
		result = ERROR_PARAMETER_INVALID;
		goto cleanUp;
	}
	is.open(argv[1]);
	if (!is.is_open())
	{
		is.close();
		std::cerr << "Failed to open file: " << argv[1];
		result = ERROR_CANNOT_OPEN_FILE;
		goto cleanUp;
	}
	while (is >> s)
	{
		try
		{
			LN a, b;
			if (s.size() == 1)
			{
				char op = s[0];
				switch (op)
				{
				case '+':
					two_previous(stack, a, b);
					add(stack, a + b);
					break;
				case '-':
					two_previous(stack, a, b);
					add(stack, a - b);
					break;
				case '*':
					two_previous(stack, a, b);
					add(stack, a * b);
					break;
				case '/':
					two_previous(stack, a, b);
					add(stack, a / b);
					break;
				case '%':
					two_previous(stack, a, b);
					add(stack, a % b);
					break;
				case '~':
					one_previous(stack, a);
					add(stack, ~a);
					break;
				case '_':
					one_previous(stack, a);
					add(stack, -a);
					break;
				case '<':
					two_previous(stack, a, b);
					add(stack, a < b);
					break;
				case '>':
					two_previous(stack, a, b);
					add(stack, a > b);
					break;
				default:
					add(stack, LN(s));
					break;
				}
			}
			else if (s == "<=")
			{
				two_previous(stack, a, b);
				add(stack, a <= b);
			}
			else if (s == ">=")
			{
				two_previous(stack, a, b);
				add(stack, a >= b);
			}
			else if (s == "==")
			{
				two_previous(stack, a, b);
				add(stack, a == b);
			}
			else if (s == "!=")
			{
				two_previous(stack, a, b);
				add(stack, a != b);
			}
			else
			{
				add(stack, LN(s));
			}
		} catch (std::bad_alloc& e)
		{
			std::cerr << "Failed to allocate memory";
			result = ERROR_OUT_OF_MEMORY;
			goto cleanUp;
		} catch (std::exception& e)
		{
			std::cerr << "Something went wrong";
			result = ERROR_UNKNOWN;
			goto cleanUp;
		}
	}
	os.open(argv[2]);
	if (!os.is_open())
	{
		os.close();
		std::cerr << "Failed to open file: " << argv[2];
		result = ERROR_CANNOT_OPEN_FILE;
		goto cleanUp;
	}
	for (size_t i = 0; i < stack.size(); ++i)
	{
		stack[stack.size() - i - 1].print(os);
	}
cleanUp:
	is.close();
	os.close();
	return result;
}