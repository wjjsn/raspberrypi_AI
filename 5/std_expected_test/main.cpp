#include <iostream>
#include <expected>

std::expected<int, std::string> f(bool b)
{
	if (b)
		return 10;
	else
		return std::unexpected("something wrong");
}

int main()
{
	auto result = f(false);
	if (result)
		std::cout << "Result: " << *result << std::endl;
	else
		std::cerr << "Error: " << result.error() << std::endl;
	return 0;
}