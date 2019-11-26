namespace vcrtl {

template <typename ForwardIt, typename T>
bool binary_search(ForwardIt first, ForwardIt last, T const & value)
{
	while (first != last)
	{
		ForwardIt mid = first + (last - first) / 2;
		if (value < *mid)
			last = mid;
		else if (*mid < value)
			first = ++mid;
		else
			return true;
	}

	return false;
}

}

#pragma once
