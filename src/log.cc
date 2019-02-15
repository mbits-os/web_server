#include "web/log.h"
#include <ctime>
#include <chrono>
#include <cstring>
#include <iomanip>

#ifdef POSIX
// #include <sys/types.h>
// #include <unistd.h>
#define HAS_COLORS 1
#else
#define HAS_COLORS 1
#endif

#include <atomic>

static std::atomic<unsigned> next_thid{ 0 };

unsigned get_next_thid() {
	auto expected = next_thid.load();
	auto next = expected + 1;
	while (!next_thid.compare_exchange_weak(expected, next))
		next = expected + 1;
	return expected;
}

unsigned get_thid() {
	thread_local auto thid = get_next_thid();
	return thid;
}

// define USE_CONSOLE
#define USE_FILE

#ifdef _WIN32
#define USE_ODS
#endif

static const char* shorten(const char* path) {
	auto slash = std::strrchr(path, '/');
	if (slash)
		path = slash + 1;
	slash = std::strrchr(path, '\\');
	if (slash)
		path = slash + 1;
	return path;
}

int logging::verbose_ = 0;

std::string logging::init_line(bool with_colors) {
	using clock = std::chrono::system_clock;
	auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
		clock::now().time_since_epoch());
	auto secs = std::chrono::duration_cast<std::chrono::seconds>(now);
	now -= secs;

	auto time = clock::to_time_t(clock::time_point{ secs });
	auto local = *std::localtime(&time);

	std::ostringstream prefix;
	prefix << std::put_time(&local, "%m-%d %H:%M:%S") << '.' << std::setfill('0')
		<< std::setw(3) << now.count() << " [";

	auto const id = get_thid();
	if (id) {
		char buffer[20];
		snprintf(buffer, sizeof(buffer), "#%u", id);
		prefix << std::right << std::setw(5) << std::setfill(' ') << buffer << ' ';
	} else {
		prefix << " Main ";
	}

	prefix << "] ";

	const char* color = "";
	switch (lvl_) {
	case debug: {
#if HAS_COLORS
		if (with_colors) {
			color = verbosity_ == 0
				? "\033[2;49;32m"
				: verbosity_ == 1 ? "\033[0;49;36m" : "\033[0;49;34m";
		}
#endif
		const char* cat =
			verbosity_ == 0 ? "INF" : verbosity_ == 1 ? "DBG" : "VRB";
		prefix << color << cat;
		if (verbosity_ > 2)
			prefix << '/' << verbosity_;
		break;
	}
	case warning:
#if HAS_COLORS
		if (with_colors)
			color = "\033[1;49;33m";
#endif
		prefix << color << "WRN";
		break;
	case error:
#if HAS_COLORS
		if (with_colors)
			color = "\033[1;49;31m";
#endif
		prefix << color << "ERR";
		break;
	}

#if HAS_COLORS
	if (with_colors)
		prefix << " \033[1;49;30m[" << shorten(file_) << ':' << line_
			<< "]\033[0m > ";
	else
#endif
		prefix << " [" << shorten(file_) << ':' << line_ << "] > ";
	return prefix.str();
}

void logging::output() {
	if (!dirty_)
		return;

	auto s = os_.str();
	if (s.empty() || s[s.length() - 1] != '\n')
		s.push_back('\n');
	output(s);
}

#ifdef USE_FILE
FILE* get_logfile() {
	struct fcloser {
		void operator()(FILE* f) { std::fclose(f); }
	};

	static std::unique_ptr<FILE, fcloser> logfile{ fopen("web.log", "w") };
	return logfile.get();
}
#endif

#ifdef USE_ODS
#include "Windows.h"
#endif

void logging::output(const std::string& message) {
#ifdef USE_CONSOLE
	{
		auto line = init_line(true) + message;
		fwrite(line.c_str(), 1, line.length(), stdout);
		fflush(stdout);
	}
#endif
#ifdef USE_FILE
	{
		auto line = init_line(false) + message;
		auto logfile = get_logfile();
		fwrite(line.c_str(), 1, line.length(), logfile);
		fflush(logfile);
	}
#endif
#ifdef USE_ODS
	{
		std::ostringstream prefix;
		prefix << file_ << '(' << line_ << "): ";
		switch (lvl_) {
		case debug: {
			prefix << (verbosity_ == 0 ? "INF" : verbosity_ == 1 ? "DBG" : "VRB");
			if (verbosity_ > 2)
				prefix << '/' << verbosity_;
			break;
		}
		case warning:
			prefix << "WRN";
			break;
		case error:
			prefix << "ERR";
			break;
		}

		prefix << ": [";
		if (get_thid())
			prefix << "#" << get_thid();
		else
			prefix << "Main";
		prefix << "] > " << message;

		OutputDebugStringA(prefix.str().c_str());
	}
#endif
}
