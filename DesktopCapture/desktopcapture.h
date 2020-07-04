#pragma once

#define EXPORT __declspec(dllexport)
#define STDCALL(rt) rt __stdcall

EXPORT STDCALL(int) getInt();

