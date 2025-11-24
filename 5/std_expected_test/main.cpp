#include <iostream>
#include <expected>
#include <string_view>

std::expected<int, std::string_view> f(bool b)
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