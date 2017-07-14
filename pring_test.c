#include <stdio.h>

#include "cycle.h"
#include "pring.h"

#include <assert.h>

static struct ck_pring_elt buf[4096];
static struct ck_pring pring = CK_PRING_INIT(buf);

uintptr_t
ck_pring_sdequeue_out(struct ck_pring *pring)
{

	return ck_pring_sdequeue(pring, 0);
}

bool
ck_pring_senqueue_out(struct ck_pring *pring, uintptr_t value)
{

	return ck_pring_senqueue(pring, value);
}


bool
ck_pring_senqueue_val_out(struct ck_pring *pring, uintptr_t value, uintptr_t *out)
{

	return ck_pring_senqueue_val(pring, value, out);
}

int
main()
{
	for (size_t j = 0; j < 100000; j++) {
		ticks begin = getticks();
		const char *name;

		switch (j % 4) {
		case 0:
		case 1:
			name = "menqueue";
			for (size_t i = 0; i < 5000; i++) {
				if (!ck_pring_menqueue(&pring, i + 1)) {
					assert(i == 4096);
					break;
				}
			}
			break;
		case 2:
			name = "senqueue";
			for (size_t i = 0; i < 5000; i++) {
				if (!ck_pring_senqueue(&pring, i + 1)) {
					assert(i == 4096);
					break;
				}
			}
			break;
		case 3:
			name = "senqueue_n";
#define BLOCK_SIZE 32
			for (size_t i = 0; i < 5000; ) {
				uintptr_t buf[BLOCK_SIZE];
				size_t n;

				for (size_t k = 0; k < BLOCK_SIZE; k++) {
					buf[k] = 1 + i + k;
				}

				n = ck_pring_senqueue_n(&pring, buf, BLOCK_SIZE);
				i += n;
				if (n < BLOCK_SIZE) {
					assert(i == 4096);
					break;
				}
			}
#undef BLOCK_SIZE
			break;
		}

		printf("%s: %.3f\n", name,
		    elapsed(getticks(), begin) / 4096);

		begin = getticks();
		switch (j % 3) {
		case 0:
			name = "mdequeue";
			for (size_t i = 0; i < 5000; i++) {
				size_t value = ck_pring_mdequeue(&pring, 0);

				if (value == 0) {
					assert(i == 4096);
					break;
				}

				assert(value == i + 1);
			}
			break;
		case 1:
			name = "sdequeue";
			for (size_t i = 0; i < 5000; i++) {
				size_t value = ck_pring_sdequeue(&pring, 0);

				if (value == 0) {
					assert(i == 4096);
					break;
				}

				assert(value == i + 1);
			}
			break;
		case 2:
			name = "sdequeue_n";
#define BLOCK_SIZE 32
			for (size_t i = 0; i < 5000; ) {
				uintptr_t values[BLOCK_SIZE];
				size_t n;

				n = ck_pring_sdequeue_n(&pring, 0,
				    &values[0], BLOCK_SIZE);
				for (size_t k = 0; k < n; k++) {
					assert(values[k] == i + k + 1);
				}

				i += n;
				if (n < BLOCK_SIZE) {
					assert(i == 4096);
					break;
				}
			}
#undef BLOCK_SIZE
			break;
		}

		printf("%s: %.3f\n", name,
		    elapsed(getticks(), begin) / 4096);
	}

	return 0;
}

/*
senqueue fast path:

        pushq   %rbp
        movq    %rsp, %rbp
        movq    8(%rdi), %r9
        movq    24(%rdi), %r8
        movq    %r9, %rcx
        andq    %r8, %rcx
        shlq    $4, %rcx
        addq    16(%rdi), %rcx
        movq    %r9, %rax
        subq    (%rdi), %rax
        cmpq    %r8, %rax
        ja      LBB2_1
        movq    %rsi, 8(%rcx)
        movq    %r9, (%rcx)
        incq    %r9
        movq    %r9, 8(%rdi)
        movb    $1, %al
        popq    %rbp
        retq

sdequeue':
        pushq   %rbp
        movq    %rsp, %rbp
        movq    64(%rdi), %r8
        movq    80(%rdi), %rcx
        movq    72(%rdi), %rdx
        andq    %rcx, %rdx
        shlq    $4, %rdx
        movq    (%r8,%rdx), %rsi
        cmpq    %rcx, %rsi
        sete    %al
        movzbl  %al, %eax
        addq    %rcx, %rax
        movq    %rax, 80(%rdi)
        movq    8(%r8,%rdx), %rdx
        xorl    %eax, %eax
        cmpq    %rcx, %rsi
        cmoveq  %rdx, %rax
        popq    %rbp
        retq


sdequeue fast path:

        pushq   %rbp
        movq    %rsp, %rbp
        movq    64(%rdi), %rax
        movq    72(%rdi), %r8
        movq    80(%rdi), %rdx
        movq    %rdx, %rsi
        andq    %r8, %rsi
        leaq    4(%rdx), %rcx
        andq    %r8, %rcx
        shlq    $4, %rcx
        prefetcht0      (%rax,%rcx)
        shlq    $4, %rsi
        movq    (%rax,%rsi), %rcx
        movq    8(%rax,%rsi), %rax
        xorl    %esi, %esi
        cmpq    %rdx, %rcx
        movq    $-1, %rcx
        cmovneq %rsi, %rcx
        cmovneq %rsi, %rax
        subq    %rcx, 80(%rdi)
        popq    %rbp
        retq

 */
