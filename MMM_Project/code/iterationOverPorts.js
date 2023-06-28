




outlets = 2;
setinletassist(0,"makeCollectionFromList");
setoutletassist(1,"length");
setoutletassist(0,"index");

/**
* This function takes a list and outputs them one at a time with "insert" prepended
**/

function jsIterationFunction()
{
	
	var i,len;
	len = arguments.length;

	for (i=0;i<len;i++) 
	{
		outlet(0,"insert",i, arguments[i]);

		
	}
	outlet(1, len);

}


