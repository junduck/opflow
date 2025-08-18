#pragma once

#ifndef OPFLOW_RESTRICT
#if defined(_MSC_VER)
#define OPFLOW_RESTRICT __restrict
#elif defined(__GNUC__) || defined(__clang__)
#define OPFLOW_RESTRICT __restrict__
#else
#define OPFLOW_RESTRICT
#endif
#endif
