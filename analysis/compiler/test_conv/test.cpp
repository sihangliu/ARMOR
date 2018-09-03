void conv() 
{
	float in[10];
	float out[10];
	float kernel[4];

	int i, j;

	for (i=0; i < 10; i++) {
		out[i]=0;
		for (j=0; j<10; j++) {
			out[i] += in[i-j] * kernel[j];
		}
	}
}
