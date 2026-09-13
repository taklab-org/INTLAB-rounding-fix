function testmm(n)
	disp('testing multiplication...')
	flag = 1;
	for k=1:n
		a=zeros(n);
		a(:,k)=ones(n,1)*1/3;
		b=zeros(n);
		b(k,:)=ones(1,n)*1/3;
		c = (rad(intval(a) * b) ~= 0) + 0.0;
		if prod(prod(c)) == 0
			disp('multiplication error!')
			flag = 0;
			break
		end
	end
	if flag == 1
		disp('multiplication OK!')
	end

	disp('testing addition...')
	flag = 1;
	for k=2:n
		a=zeros(n);
		a(:,1)=ones(n,1);
		a(:,k)=ones(n,1)*2^(-27);
		b=zeros(n);
		b(1,:)=ones(1,n);
		b(k,:)=ones(1,n)*2^(-27);
		c = (rad(intval(a) * b) ~= 0) + 0.0;
		if prod(prod(c)) == 0
			disp('addition error!')
			flag = 0;
			break
		end
	end
	if flag == 1
		disp('addition OK!')
	end
end