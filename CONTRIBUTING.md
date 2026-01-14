# CONTRIBUTING

All kinds of contributions are welcome.

Please follow the existing coding style. The project uses `clang-format` for code formatting. If you are not sure about the style of your code, you can format your changes by running:

```shell
make format
```

## Possible contribution ideas

- Optimizations
- Fixing bugs and memory leaks
- Adding new features
- Editing and adding documentation
- Writing tests

There are some possible optimaziton opportunities:

- Using `malloc`/`free` is still necessary in some parts of the core and plugins, so it might be useful to have a global server arena where we can store all the persistent memory coming from plugins.
- Body streaming is needed for huge uploads (like videos).

**Thank you for your contribution <3**
