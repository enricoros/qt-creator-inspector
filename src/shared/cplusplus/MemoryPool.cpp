/**************************************************************************
**
** This file is part of Qt Creator
**
** Copyright (c) 2009 Nokia Corporation and/or its subsidiary(-ies).
**
** Contact: Nokia Corporation (qt-info@nokia.com)
**
** Commercial Usage
**
** Licensees holding valid Qt Commercial licenses may use this file in
** accordance with the Qt Commercial License Agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and Nokia.
**
** GNU Lesser General Public License Usage
**
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL included in the
** packaging of this file.  Please review the following information to
** ensure the GNU Lesser General Public License version 2.1 requirements
** will be met: http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** If you are unsure which license is appropriate for your use, please
** contact the sales department at http://qt.nokia.com/contact.
**
**************************************************************************/
// Copyright (c) 2008 Roberto Raggi <roberto.raggi@gmail.com>
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.

#include "MemoryPool.h"
#include <cstring>
#include <cassert>

using namespace CPlusPlus;

MemoryPool::MemoryPool()
    : _initializeAllocatedMemory(true),
      _blocks(0),
      _allocatedBlocks(0),
      _blockCount(-1),
      ptr(0),
      end(0)
{ }

MemoryPool::~MemoryPool()
{
    if (_blockCount != -1) {
        for (int i = 0; i < _blockCount + 1; ++i) {
            std::free(_blocks[i]);
        }
    }

    if (_blocks)
        std::free(_blocks);
}

bool MemoryPool::initializeAllocatedMemory() const
{ return _initializeAllocatedMemory; }

void MemoryPool::setInitializeAllocatedMemory(bool initializeAllocatedMemory)
{ _initializeAllocatedMemory = initializeAllocatedMemory; }

void *MemoryPool::allocate_helper(size_t size)
{
    assert(size < BLOCK_SIZE);

    if (++_blockCount == _allocatedBlocks) {
        if (! _allocatedBlocks)
            _allocatedBlocks = 8;
        else
            _allocatedBlocks *= 2;

        _blocks = (char **) realloc(_blocks, sizeof(char *) * _allocatedBlocks);
    }

    char *&block = _blocks[_blockCount];

    if (_initializeAllocatedMemory)
        block = (char *) std::calloc(1, BLOCK_SIZE);
    else
        block = (char *) std::malloc(BLOCK_SIZE);

    ptr = block;
    end = ptr + BLOCK_SIZE;

    void *addr = ptr;
    ptr += size;
    return addr;
}

MemoryPool::State MemoryPool::state() const
{ return State(ptr, _blockCount); }

void MemoryPool::rewind(const State &state)
{
    if (_blockCount == state.blockCount && state.ptr < ptr) {
        if (_initializeAllocatedMemory)
            std::memset(state.ptr, '\0', ptr - state.ptr);

        ptr = state.ptr;
    }
}

Managed::Managed()
{ }

Managed::~Managed()
{ }

void *Managed::operator new(size_t size, MemoryPool *pool)
{ return pool->allocate(size); }

void Managed::operator delete(void *)
{ }

void Managed::operator delete(void *, MemoryPool *)
{ }


