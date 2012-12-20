/* No need to export any objects. This lib will only be used to check
   if dlopen() works. */

static void __attribute__ ((constructor)) my_init(void) {
}
