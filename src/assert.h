namespace vcrtl {

void _verify(bool cond);

template <typename E>
void verify(E && cond)
{
	_verify(static_cast<bool>(cond));
}

}

#pragma once
