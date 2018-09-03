struct st {
	float a;
	float b;
};

st func(st approx_st_1) {
	st approx_rtn;
	approx_rtn.a = approx_st_1.a * approx_st_1.a;
	//approx_rtn.b = approx_st_1.b * approx_st_1.b;
	return approx_rtn;
}

int main() {
	st approx_s1, approx_s3;
	approx_s1.a = approx_s1.a * approx_s1.a;
	//float xx;
	//xx = approx_s1.a;
	//approx_s1.b = approx_s1.b * approx_s1.b;
	approx_s3 = func(approx_s1);
}
