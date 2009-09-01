CREATE SCHEMA test;

SET search_path TO test;

CREATE TABLE test1 (
       a int PRIMARY KEY,
       b text
);

INSERT INTO test1 VALUES (1, 'one'),
       	    	  	 (2, 'two'),
			 (3, 'three');
