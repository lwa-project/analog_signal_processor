#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/numpy.h>
#include "aspCommon.hpp"
#include "ivsCommon.hpp"

namespace py = pybind11;

PYBIND11_MODULE(atmega_py, m) {
    m.doc() = "ATmega Python wrapper";
    
    py::class_<ATmega>(m, "ATmega")
        .def(py::init<std::string>())
        .def("open", &ATmega::open)
        .def("get_version", &ATmega::get_version)
        .def("transfer_spi", &ATmega::transfer_spi)
        .def("list_rs485_devices", &ATmega::list_rs485_devices)
        .def("read_rs485", &ATmega::read_rs485)
        .def("write_rs485", &ATmega::write_rs485)
        .def("send_rs485", &ATmega::send_rs485)
        .def("list_i2c_devices", &ATmega::list_i2c_devices)
        .def("read_i2c", [](ATmega& self, uint8_t addr, uint8_t reg, int length) {
            std::vector<uint8_t> data(length);
            bool success = self.read_i2c(addr, reg, (char*)data.data(), length);
            if (success) {
                return py::make_tuple(true, data);
            } else {
                return py::make_tuple(false, std::vector<uint8_t>());
            }
        })
        .def("write_i2c", [](ATmega& self, uint8_t addr, uint8_t reg, const std::vector<uint8_t>& data) {
            return self.write_i2c(addr, reg, (const char*)data.data(), data.size());
        })
        .def("clear_fault", &ATmega::clear_fault)
        .def("locate", &ATmega::locate)
        .def("reset", &ATmega::reset);
    
    // Wrap free functions from aspCommon
    m.def("list_atmegas", &list_atmegas, "List all ATmega serial numbers");
    
    // Wrap the low-level atmega namespace functions if needed
    m.def("find_devices", &atmega::find_devices, "Find all available ATmega devices");
    m.def("strerror", &atmega::strerror, "Decode error message");
    
    // Wrap the buffer structure for low-level access if needed
    py::class_<atmega::buffer>(m, "Buffer")
        .def(py::init<>())
        .def_readwrite("command", &atmega::buffer::command)
        .def_readwrite("size", &atmega::buffer::size)
        .def_property("buffer", 
            [](const atmega::buffer& self) {
                return py::bytes((char*)self.buffer, self.size);
            },
            [](atmega::buffer& self, py::bytes data) {
                std::string str = data;
                self.size = std::min((size_t)ATMEGA_MAX_BUFFER_SIZE, str.size());
                std::memcpy(self.buffer, str.data(), self.size);
            });
    
    // Wrap command enum
    py::enum_<atmega::Command>(m, "Command")
        .value("SUCCESS", atmega::COMMAND_SUCCESS)
        .value("READ_SN", atmega::COMMAND_READ_SN)
        .value("READ_VER", atmega::COMMAND_READ_VER)
        .value("READ_MLEN", atmega::COMMAND_READ_MLEN)
        .value("ECHO", atmega::COMMAND_ECHO)
        .value("TRANSFER_SPI", atmega::COMMAND_TRANSFER_SPI)
        .value("SCAN_RS485", atmega::COMMAND_SCAN_RS485)
        .value("READ_RS485", atmega::COMMAND_READ_RS485)
        .value("WRITE_RS485", atmega::COMMAND_WRITE_RS485)
        .value("SEND_RS485", atmega::COMMAND_SEND_RS485)
        .value("SCAN_I2C", atmega::COMMAND_SCAN_I2C)
        .value("READ_I2C", atmega::COMMAND_READ_I2C)
        .value("WRITE_I2C", atmega::COMMAND_WRITE_I2C)
        .value("LOCK", atmega::COMMAND_LOCK)
        .value("UNLOCK", atmega::COMMAND_UNLOCK)
        .value("WRITE_SN", atmega::COMMAND_WRITE_SN)
        .value("CLR_FAULT", atmega::COMMAND_CLR_FAULT)
        .value("LOCATE", atmega::COMMAND_LOCATE)
        .value("RESET", atmega::COMMAND_RESET)
        .value("FAILURE", atmega::COMMAND_FAILURE)
        .value("FAILURE_ARG", atmega::COMMAND_FAILURE_ARG)
        .value("FAILURE_STA", atmega::COMMAND_FAILURE_STA)
        .value("FAILURE_BUS", atmega::COMMAND_FAILURE_BUS)
        .value("FAILURE_RS485", atmega::COMMAND_FAILURE_RS485)
        .value("FAILURE_TOUT", atmega::COMMAND_FAILURE_TOUT)
        .value("FAILURE_CMD", atmega::COMMAND_FAILURE_CMD);
    
    // Wrap iVS helper functions as well
    m.def("ivs_get_smart_modules", &ivs_get_smart_modules);
    m.def("ivs_select_module", &ivs_select_module);
    m.def("ivs_wait_not_busy", &ivs_wait_not_busy);
    m.def("ivs_is_on", &ivs_is_on);
    m.def("ivs_enable_all_writes", &ivs_enable_all_writes);
    m.def("ivs_disable_writes", &ivs_disable_writes);
}
