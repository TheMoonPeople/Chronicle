#pragma once

/**
 * @file
 * Names the allocator template without pulling its definition in.
 *
 * MWCC rejects a template declared twice in one translation unit, so every
 * header that only needs the name includes this one rather than repeating it.
 */
template <int Kind>
class CDataAlloc2;
