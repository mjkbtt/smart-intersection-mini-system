#pragma once
// secrets.h байхгүй үед ч compile хийгдэхийн тулд жишээ утгыг ашиглана.
#if __has_include("secrets.h")
#include "secrets.h"
#else
#warning "secrets.h олдсонгүй: secrets.example.h-ийн жишээ утгыг ашиглаж байна"
#include "secrets.example.h"
#endif
