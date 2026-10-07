#include "cli.hpp"

#include "phase2/interpret.hpp"
#include "printer.hpp"
#include "bytecode/parser.hpp"
#include "bytecode/prettyprinter.hpp"
#include "bytecode/compiler.hpp"
#include "bytecode/vm.hpp"
#include <iostream>

static std::string read_istream(std::istream &is)
{
	return std::string(std::istreambuf_iterator<char>(is),
					   std::istreambuf_iterator<char>());
}

int main(int argc, char **argv)
{
	Command command = cli_parse(argc, argv);

	std::string contents = read_istream(*command.input_stream);
	std::string input_filename = command.input_filename;
	std::string output_filename = command.output_filename;

	switch (command.kind)
	{
	case CommandKind::SCAN:
	{
		std::vector<lexcode::LexToken> tokens = lexcode::lex(contents);
		*command.output_stream << printer::print_lex(tokens);
		break;
	}
	case CommandKind::PARSE:
	{
		auto tree = parsecode::parse(contents);
		*command.output_stream << printer::print_ast(*tree) << std::endl;
		// delete tree;
		break;
	}
	case CommandKind::COMPILE:
	{
		auto tree = parsecode::parse(contents);
		// 2) Compile AST → bytecode::Function*
		BytecodeCompiler compiler;
		compiler.setOptimizations(command.opt);
		bytecode::Function *root = compiler.compile(*tree);

		// 3) Pretty print to the chosen output stream
		bytecode::prettyprint(root, *command.output_stream);
		*command.output_stream << std::endl;
		break;
	}
	case CommandKind::INTERPRET:
		interpretcode::interpret(contents, command.mem * 1024 * 1024);
		break;
	case CommandKind::VM:
	{
		auto bytecode_func = bytecode::parse(contents);

		// Check if parsing succeeded
		if (!bytecode_func)
		{
			std::cerr << "Error: Failed to parse bytecode file" << std::endl;
			return 1;
		}

		try
		{
			// Create heap with memory limit
			CollectedHeap heap(command.mem * 1024 * 1024);

			// Create and run VM
			bytecode::VirtualMachine vm(heap);
			vm.execute(bytecode_func);
		}
		catch (const std::exception &e)
		{
			std::cerr << "VM Error: " << e.what() << std::endl;
			return 1;
		}
		break;
	}
	case CommandKind::DERBY:
	{
		// Parse MITScript source code to AST
		auto tree = parsecode::parse(contents);

		// Compile AST to bytecode
		BytecodeCompiler compiler;
		compiler.setOptimizations(command.opt);
		bytecode::Function *root = compiler.compile(*tree);

		// Create heap with memory limit
		CollectedHeap heap(command.mem * 1024 * 1024);

		// Create and run VM with full optimizations
		bytecode::VirtualMachine vm(heap);

		try
		{
			vm.execute(root);
		}
		catch (const std::exception &e)
		{
			std::cerr << "VM Error: " << e.what() << std::endl;
			return 1;
		}

		break;
	}
	}

	return 0;
}
