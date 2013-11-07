#if OPT_CONTEXT_THREADING
#define NEXT() do { pc++; __asm__ __volatile__ ("ret" : : "r" (pc)); } while(0)
#elif OPT_DIRECT_THREADING
#define NEXT() goto **pc++
#else
#error unsupported value for "CALL_MODEL"
#endif

static void **
#if OPT_CONTEXT_THREADING
test_context_threading(void ** stream)
#else
test_direct_threading(void ** stream)
#endif
{
  register void ** pc __asm__("r14");

#define L(n) &&l##n
#define LABELS(n) L(n##0), L(n##1), L(n##2), L(n##3), L(n##4), L(n##5), L(n##6), L(n##7), L(n##8), L(n##9)
  if (0 == stream) {
    static void * table[] = {
      LABELS(0),
      LABELS(1),
      LABELS(2),
      LABELS(3),
      LABELS(4),
      LABELS(5),
      LABELS(6),
      LABELS(7),
      LABELS(8),
      LABELS(9),
      &&term
    };
    return table;
  }
  pc = stream;
  goto **pc++;

#define INSTRUCTION(n) l##n: NEXT()
#define INSTRUCTIONS(n) \
  INSTRUCTION(n##0); \
  INSTRUCTION(n##1); \
  INSTRUCTION(n##2); \
  INSTRUCTION(n##3); \
  INSTRUCTION(n##4); \
  INSTRUCTION(n##5); \
  INSTRUCTION(n##6); \
  INSTRUCTION(n##7); \
  INSTRUCTION(n##8); \
  INSTRUCTION(n##9)

  INSTRUCTIONS(0);
  INSTRUCTIONS(1);
  INSTRUCTIONS(2);
  INSTRUCTIONS(3);
  INSTRUCTIONS(4);
  INSTRUCTIONS(5);
  INSTRUCTIONS(6);
  INSTRUCTIONS(7);
  INSTRUCTIONS(8);
  INSTRUCTIONS(9);
term:
  return NULL;
}

#undef NEXT

