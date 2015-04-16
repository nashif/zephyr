/* From: http://fossies.org/dox/musl-1.0.5/isalpha_8c_source.html */
int isalpha(int c)
{
	return ((unsigned)c|32)-'a' < 26;
}
