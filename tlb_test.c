/*
  Copyright (c) 2015, Cosmin Gorgovan
  All rights reserved.

  Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

  1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

  2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

  3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <sys/mman.h>
#include <stdint.h>
#include <string.h>

#define PAGE_SIZE 4096
#define OP_CNT (1000*1000*1000)
#define OVERSIZE 8
#define MAX_SIZE 2048
#define INST_OFFSET (PAGE_SIZE + 8)

extern void data_test(void *buf, int page_cnt, int op_cnt);
extern void *inst_test;

typedef void (*itest)(uint32_t count);

void help() {
  printf("\nSyntax: ./tlb_test [d|i] SIZE [-huge]\n"
         "  d      - dTLB test\n"
         "  i      - iTLB test\n"
         "  SIZE   - specified in 4KiB units [1...%d]\n"
         "  -huge  - allocates huge pages\n\n", MAX_SIZE);
  exit(EXIT_FAILURE);
}

void prepare_inst(void *buf, int cnt) {
  uint32_t *fixup;
  void *start_buf = buf;

  for (int i = 0; i < cnt; i++) {
    memcpy(buf, &inst_test, 12);
    buf += INST_OFFSET;
  }

  // Loop back to the first page
  fixup = ((uint32_t *)(buf - INST_OFFSET)) + 1;
  *fixup &= 0xFF000000;
  *fixup |= ((uint32_t *)start_buf - fixup - 2) & 0xFFFFFF;

  __clear_cache(start_buf, fixup + 3);
}

int main(int argc, char **argv) {
  int page_cnt;
  uint8_t *buf;
  int use_huge_pages = 0;
  int is_data_test;
  itest itlb_test;

  if (argc != 3 && argc != 4) help();

  if (strcmp(argv[1], "d") == 0) {
    is_data_test = 1;
  } else if (strcmp(argv[1], "i") == 0) {
    is_data_test = 0;
  } else {
    help();
  }

  page_cnt = atoi(argv[2]);
  if (page_cnt < 1 || page_cnt > MAX_SIZE) help();

  if (argc == 4) {
    if (strcmp(argv[3], "-huge") == 0) {
      use_huge_pages = 1;
    } else {
      help();
    }
  }

  buf = mmap(NULL, PAGE_SIZE * (page_cnt + OVERSIZE),
             PROT_READ | PROT_WRITE | (is_data_test ? 0 : PROT_EXEC),
             MAP_PRIVATE|MAP_ANONYMOUS|(use_huge_pages ? MAP_HUGETLB : 0), -1, 0);
  assert(buf != MAP_FAILED);

  if (is_data_test) {
    data_test(buf, page_cnt, OP_CNT);
  } else {
    prepare_inst(buf, page_cnt);
    itlb_test = (itest)buf;
    itlb_test(OP_CNT);
  }

  return 0;
}

