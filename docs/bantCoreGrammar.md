```
<type> ::= 	<ident>
		| <type> '->' <type>
		| '('[<type>[','<type>]*]')' '->' <type>
		| 'List' '[' <type> ']'
		| 'Tuple' '[' <type> ']'
		| 'type' <ident>

<op> ::= 	[ '*' | '/' | '+' | '-' | '<' | '>' | '!' | '==' | '!=' | '<=' | '>=' | '&&' | '||' ]

<int> ::= 	[ integer ]

<bool> ::= 	'true' | 'false'

<char> ::= 	'''[ character ]'''

<string> ::= 	'"'[ character* ]'"'

<atom> ::= 	<int> 
		| <bool> 
		| 'null' 
		| <char> 
		| <string>
		| '('<simp>')'
		| <ident>['.'<ident>]*

<tight> ::= 	<atom>[ ['['<type>[','<type>]*']']* ['('[<simp>[','<simp>]*]')']* ] 
		| [ '('<simp>')' ]+
		| '{'<exp>'}'

<utight> ::= 	[<op>]<tight>

<arg> ::= 	<ident>':'<type>['='<atom>]

<typeclass> ::= 'type' <ident> '{' [<ident>':'<type>[','<ident>':'<type>]*]* '}'

<prog> ::= 	['func' <ident>['['<type> [',' <type> ]* ']'] '('[<arg>[','<arg>]*]')'['->' <type>] '=' <simp>';' ]* 

<simp> ::= 	<utight>[<op><utight>]*
		| 'if' '('<simp>')' <simp> ['else' <simp>]
		| 'List' '{'[<simp>[','<simp>]*]'}'
		| 'Tuple '{'[<simp>[','<simp>]*]'}'
		| 'match' '('<ident>')' '{' ['case' <atom> '=' { <simp> }; ]* ['case' 'any' '=' { <simp> }; ] '}'
		| <typeclass>
		| <prog> <exp>

<exp> ::= 	<simp>[;<exp>]
		| 'val' <ident> ':' <type> '=' <simp>';' <exp>
```
