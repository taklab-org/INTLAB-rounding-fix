function report=check_intlab_enclosures
% Independent exact oracles for real dense double interval products.
originalRound=getround; originalThreads=maxNumCompThreads;
restore=onCleanup(@() restoreState(originalRound,originalThreads));
randomState=rng; restoreRandom=onCleanup(@() rng(randomState)); rng(1937);
sizes=[2 17 127 287 288 512 540 1024];
limits=unique([1 2 4 originalThreads],'stable');
cases=0; sampled=0; fullEntries=0;
for threads=limits
    maxNumCompThreads(threads);
    for n=sizes
        rounding_patch_check(n);
        setround(0);
        A=zeros(n); B=zeros(n);
        A(:,1)=1; A(:,2)=2^-27; B(1,:)=1; B(2,:)=2^-27;
        for sign=[1 -1]
            if sign==1, lower=1; upper=1+eps;
            else, lower=-1-eps; upper=-1; end
            for kind=1:3
                switch kind
                    case 1, C=intval(sign*A)*B;
                    case 2, C=(sign*A)*intval(B);
                    case 3, C=intval(sign*A)*intval(B);
                end
                lo=inf(C); hi=sup(C);
                assert(all(isfinite(lo(:)) & isfinite(hi(:)) & ...
                    lo(:)<=lower & hi(:)>=upper),'Exact point enclosure failed.');
                cases=cases+1; fullEntries=fullEntries+numel(lo);
            end
        end
    end
    % Signed nonpoint intervals, all transpose combinations, rectangular.
    % Integer endpoint products and sums fit exactly in binary64 (<2^53).
    % Reference uses scalar endpoint products, never a BLAS matrix product.
    for shape=[31 513]
        m=shape; n=shape+7; k=shape+3;
        for ta=0:1
            for tb=0:1
                setround(0);
                al=randi([-8 7],m,k); ah=al+randi([0 3],m,k);
                bl=randi([-8 7],k,n); bh=bl+randi([0 3],k,n);
                if ta, X=infsup(al'/16,ah'/16)'; else, X=infsup(al/16,ah/16); end
                if tb, Y=infsup(bl'/16,bh'/16)'; else, Y=infsup(bl/16,bh/16); end
                C=X*Y; lo=inf(C); hi=sup(C);
                for sample=1:64
                    row=randi(m); col=randi(n);
                    products=[al(row,:).*bl(:,col)'; al(row,:).*bh(:,col)'; ...
                        ah(row,:).*bl(:,col)'; ah(row,:).*bh(:,col)'];
                    lower=sum(min(products,[],1))/256;
                    upper=sum(max(products,[],1))/256;
                    assert(isfinite(lo(row,col)) && isfinite(hi(row,col)) && ...
                        lo(row,col)<=lower && hi(row,col)>=upper, ...
                        'Exact nonpoint enclosure failed.');
                    sampled=sampled+1;
                end
                cases=cases+1;
            end
        end
    end
    fprintf('INTLAB_ENCLOSURES threads=%d cumulative_cases=%d passed=1\n',threads,cases);
end
report=struct('passed',true,'cases',cases,'fullEntries',fullEntries, ...
    'sampledNonpointEntries',sampled,'sizes',sizes,'threadLimits',limits);
end
function restoreState(rounding,threads)
setround(rounding); maxNumCompThreads(threads);
end
