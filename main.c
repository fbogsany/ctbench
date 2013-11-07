#include <stdio.h>
#include <stdlib.h>
#include <mach/mach_time.h>
#include <sys/mman.h>
#include <limits.h>

#define LAST_INSTRUCTION 100
typedef unsigned long long VALUE;
static const VALUE TWO_MB = 2 * 1024 * 1024;

#define OPT_CONTEXT_THREADING 1
#include "test.h"
#undef OPT_CONTEXT_THREADING

#define OPT_DIRECT_THREADING 1
#include "test.h"
#undef OPT_DIRECT_THREADING

static int*
sequence(int count, int limit)
{
  int * result = (int *) malloc(sizeof(int) * count);
  int i;
  for (i = 0; i < count; i++) {
    result[i] = (int) random() % limit;
  }
  result[count - 1] = LAST_INSTRUCTION;
  return result;
}

static inline unsigned char *
call_instruction_body(unsigned char *code, void * body) /* size: 5 */
{
    *code++ = 0xe8;
    *(int *)code = (int)((unsigned char *)(body) - code - 4);
    return code + 4;
}

static inline unsigned char *
jump_to_instruction_body(unsigned char *code, void * body) /* size: 5 */
{
    *code++ = 0xe9;
    *(int *)code = (int)((unsigned char *)(body) - code - 4);
    return code + 4;
}

static void
get_instruction_target_range(VALUE *start_address, VALUE *end_address)
{
    static VALUE first_instruction_address = 0;
    static VALUE last_instruction_address = 0;
    if (0 == first_instruction_address) {
        int i;
        void ** table = test_context_threading(NULL);
        first_instruction_address = last_instruction_address = (VALUE) table[0];
        for (i = 1; i <= LAST_INSTRUCTION; i++) {
            if (first_instruction_address > (VALUE) table[0]) {
                first_instruction_address = (VALUE) table[0];
            }
            if (last_instruction_address < (VALUE) table[0]) {
                last_instruction_address = (VALUE) table[0];
            }
        }
    }
    *start_address = first_instruction_address;
    *end_address = last_instruction_address;
}

static int
is_valid_context_threading_region(void *start, VALUE size)
{
    VALUE s = (VALUE) start;
    VALUE e = s + size;
    VALUE first_instruction_address, last_instruction_address;
    get_instruction_target_range(&first_instruction_address, &last_instruction_address);
    if (s > last_instruction_address) {
        return (e - first_instruction_address) < INT_MAX;
    } else {
        return (last_instruction_address - s) < INT_MAX;
    }
}

static void *
alloc_context_threading_table(void)
{
    VALUE start_region_range, end_region_range, region_address;
    VALUE first_instruction_address, last_instruction_address;

    get_instruction_target_range(&first_instruction_address, &last_instruction_address);
    start_region_range = (last_instruction_address + INT_MIN + TWO_MB - 1) & ~(TWO_MB - 1);
    end_region_range = (first_instruction_address + INT_MAX - TWO_MB) & ~(TWO_MB - 1);
    if (start_region_range > last_instruction_address) {
        start_region_range = 0;
    }
    if (end_region_range < first_instruction_address) {
        end_region_range = (SIZE_MAX - TWO_MB) & ~(TWO_MB - 1);
    }
    printf("trying to find space between %llx and %llx\n", start_region_range, end_region_range);
    for (region_address = start_region_range; region_address != end_region_range; region_address += TWO_MB) {
      printf("trying %llx\n", region_address);
        void *address = mmap((void *) region_address, TWO_MB, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
        if (address == (void *) region_address || is_valid_context_threading_region(address, TWO_MB)) {
          printf("using %p\n", address);
          return address;
        } else if (address != MAP_FAILED) {
          printf("unmapping %p\n", address);
          munmap(address, TWO_MB);
        }
    }
    return NULL;
}

static void **
context_threading_table(int * seq, unsigned int count)
{
  void ** table = test_context_threading(NULL);
  void * ctt = alloc_context_threading_table();
  void ** result = (void **) malloc(sizeof(void *));
  unsigned char * code = (unsigned char *) ctt;
  int i;
  for (i = 0; i < count; i++) {
    if (seq[i] == LAST_INSTRUCTION) {
      code = jump_to_instruction_body(code, table[seq[i]]);
    } else {
      code = call_instruction_body(code, table[seq[i]]);
    }
  }
  mprotect(ctt, TWO_MB, PROT_READ | PROT_EXEC);
  *result = ctt;
  return result;
}

static void **
direct_threading_table(int * seq, unsigned int count)
{
  void ** table = test_direct_threading(NULL);
  void ** result = (void **) malloc(sizeof(void *) * count);
  int i;
  for (i = 0; i < count; i++) {
    result[i] = table[seq[i]];
  }
  return result;
}

static uint64_t
bench(void ** stream, void ** (*test)(void **))
{
  uint64_t start, end;
  int i;

  test(stream);
  start = mach_absolute_time();
  for (i = 0; i < 1000; i++) {
    test(stream);
  }
  end = mach_absolute_time();
  return end - start;
}

int main(int argc, char ** argv)
{
  const unsigned int SEQUENCE_COUNT = 1000;
  int * seq = sequence(SEQUENCE_COUNT, LAST_INSTRUCTION);
  printf("testing direct threading\n");fflush(NULL);
  uint64_t dt_time = bench(direct_threading_table(seq, SEQUENCE_COUNT), test_direct_threading);
  printf("testing context threading\n");fflush(NULL);
  uint64_t ct_time = bench(context_threading_table(seq, SEQUENCE_COUNT), test_context_threading);
  printf("context threading: %lld ns\n", ct_time);
  printf("direct threading: %lld ns\n", dt_time);
  return 0;
}
