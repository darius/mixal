/^#/    { next }        # comment

{
    mnemonic = $1; C = $2; F = $3; is_extended = $4
    if (F == "") F = 5		# default field: (0:5)

    flags = is_extended ? "extended" : "0"
    if (C == "x")
        printf("\tdef_directive(\"%s\", do_%s),\n", mnemonic, mnemonic)
    else
        printf("\tdef_opcode(\"%s\", %d, %d, %s),\n", mnemonic, C, F, flags)
}
