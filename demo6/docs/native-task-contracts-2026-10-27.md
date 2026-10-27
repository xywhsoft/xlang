# Native task contract completion

The compiler lifts non-null or flow-refined arguments into a nullable parameter before callback capture, using the ordinary explicit conversion. Tagged scalar frames retain their exact C representation. `task.all` boxes absent nullable strings as the existing null value and preserves native scalar width, signedness and floating bits. Ordinary non-null string representation and runtime dispatch remain unchanged.

Generic `Future.then/catch/finally` specialization substitutes and revalidates both source and destination payload types and ABIs. No dynamic generic dispatcher or extension forwarding wrapper is introduced.

Initial Windows validation: source candidate driver-46, all three new regression files passed emitted C, JIT, AOT build and AOT execution (12 stages). The optional-scalar fixture covers all 20 C scalar forms, platform extrema, NaN and negative zero. The corrected previous audit cases also pass for these four gap classes. Linux and final public boundary gates will be recorded after the generic callable stage.

`task.all` still uses its existing array element boxing. This change does not establish zero overhead for an entire asynchronous or aggregate call.
