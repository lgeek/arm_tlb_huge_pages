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
#define OVERSIZE 4
#define MAX_SIZE 2048

extern void test(void *buf, int page_cnt, int op_cnt);

void help() {
  printf("\nSyntax: ./tlb_test SIZE [-huge]\n"
         "  SIZE   - specified in 4KiB units [1...%d]\n"
         "  -huge  - allocates huge pages\n\n", MAX_SIZE);
  exit(EXIT_FAILURE);
}

int main(int argc, char **argv) {
  int page_cnt;
  uint8_t *buf;
  int use_huge_pages = 0;
  
  if (argc != 2 && argc != 3) help();
  page_cnt = atoi(argv[1]);
  if (page_cnt < 1 || page_cnt > MAX_SIZE) help();
  if (argc == 3) {
    if (strcmp(argv[2], "-huge") == 0) {
      use_huge_pages = 1;
    } else {
      help();
    }
  }
  
  buf = mmap(NULL, PAGE_SIZE * (page_cnt + OVERSIZE), PROT_READ | PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS|(use_huge_pages ? MAP_HUGETLB : 0), -1, 0);
  assert(buf != MAP_FAILED);
  
  test(buf, page_cnt, OP_CNT);

  return 0;
}

