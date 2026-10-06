// nanobind example from docs
// let's just check the build system works
// #include <nanobind/nanobind.h>
#include "freq.h"
#include "nanobind/nanobind.h"
#include <TimeTagger.h>
#include <cstdint>
#include <nanobind/ndarray.h>
#include <nanobind/stl/bind_vector.h>
#include <nanobind/stl/pair.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/unordered_map.h>
#include <nanobind/stl/vector.h>
#include <python3.13/abstract.h>
#include <python3.13/longobject.h>
#include <python3.13/object.h>
#include <stdexcept>
#include <string>
#include <vector>

namespace nb = nanobind;
using namespace nb::literals;

uintptr_t extract_swig_this_address(const nb::object &py_obj) {
    if (!nb::hasattr(py_obj, "this")) {
        throw std::invalid_argument(
            "tagger object has no '.this' attribute, expected a SWIG-wrapped timetagger object"
        );
    }

    nb::object this_attr = py_obj.attr("this");

    PyObject *py_int = PyNumber_Long(this_attr.ptr());
    if (py_int == nullptr) {
        throw nb::python_error();
    }

    auto addr = static_cast<uintptr_t>(PyLong_AsUnsignedLong(py_int));
    Py_DecRef(py_int);
    return addr;
}

std::string extract_swig_type_name(const nb::object &py_obj) {
    return nb::cast<std::string>(nb::str(py_obj.attr("__class__").attr("__name__")));
}

TimeTaggerBase *tagger_base_from_swig_object(void *stored_ptr, const std::string &type_name) {
    
    if (type_name == "TimeTaggerNetwork") {
        return static_cast<TimeTaggerBase*>(reinterpret_cast<TimeTaggerNetwork*>(stored_ptr));
    }


    if (type_name == "TimeTaggerVirtual") {
        return static_cast<TimeTaggerBase*>(reinterpret_cast<TimeTaggerVirtual*>(stored_ptr));
    }

    if (type_name == "TimeTagger") {
        return static_cast<TimeTaggerBase*>(reinterpret_cast<TimeTagger*>(stored_ptr));
    }

    throw std::invalid_argument(
        "Unsupported tagger type '" + type_name +
        "'; expected TimeTagger, "
        "TimeTaggerNetwork, or TimeTaggerVirtual"
    );
}

NB_MODULE(grater, m) {

    // Module docstring
    m.doc() = "Gravites - High-performance photon correlation and timetrace analysis for "
              "TimeTagger devices";

    nb::enum_<Size>(m, "BufferSize")
        .value("Small", Size::Small)
        .value("Medium", Size::Medium)
        .value("Large", Size::Large);

    nb::class_<IteratorBase>(m, "IteratorBase")
        .def("start", &IteratorBase::start)
        .def(
            "startFor",
            &IteratorBase::startFor,
            nb::arg("capture_duration"),
            nb::arg("clear") = true
        )
        .def("stop", &IteratorBase::stop)
        .def("abort", &IteratorBase::abort)
        .def("clear", &IteratorBase::clear)
        .def("isRunning", &IteratorBase::isRunning)
        .def("waitUntilFinished", &IteratorBase::waitUntilFinished, nb::arg("timeout") = -1)
        .def("getCaptureDuration", &IteratorBase::getCaptureDuration)
        .def("getConfiguration", &IteratorBase::getConfiguration);

    nb::class_<PatternCounter, IteratorBase>(
        m, "PatternCounter", "Count mulitple detection patterns"
    )
        .def(
            "__init__",
            [](PatternCounter *self,
               // uintptr_t tagger_ptr,
               const nb::object& tagger_pyobj, // unused except to anchor lifetime
               std::vector<channel_t> channels,
               u64 coincidence_window,
               Size buffer_size,
               int max_length) {
                // tagger.this is a std::shared_ptr<TimeTaggerNetwork>*, not a TimeTaggerNetwork*.
                // libstdc++'s shared_ptr<T> layout starts with the stored T* as its first member,
                // so the first pointer-sized word at this address is the actual object pointer.
                // void *stored_ptr = *reinterpret_cast<void **>(tagger_ptr);

                // auto *leaf = reinterpret_cast<TimeTaggerNetwork *>(stored_ptr);
                // // TimeTaggerNetwork uses virtual multiple inheritance from TimeTaggerBase,
                // // so a compiler-driven static_cast (not reinterpret_cast) is needed to apply
                // // the correct offset to the TimeTaggerBase subobject.
                // TimeTaggerBase *tagger = static_cast<TimeTaggerBase *>(leaf);

                uintptr_t shared_ptr_addr = extract_swig_this_address(tagger_pyobj);
                void *stored_ptr = *reinterpret_cast<void **>(shared_ptr_addr);
                std::string type_name = extract_swig_type_name(tagger_pyobj);
                TimeTaggerBase *tagger = tagger_base_from_swig_object(stored_ptr, type_name);

                new (self)
                    PatternCounter(tagger, channels, coincidence_window, buffer_size, max_length);
            },
            // nb::arg("tagger_ptr"),
            nb::arg("tagger_pyobj"),
            nb::arg("channels"),
            nb::arg("coincidence_window"),
            nb::arg("buffer_size") = Size::Small,
            nb::arg("max_length") = 2,
            nb::keep_alive<1, 2>()
        )
        .def("get_data", &PatternCounter::get_data);
}
