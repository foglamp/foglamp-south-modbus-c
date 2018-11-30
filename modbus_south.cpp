/*
 * FogLAMP south service plugin
 *
 * Copyright (c) 2018 OSIsoft, LLC
 *
 * Released under the Apache 2.0 Licence
 *
 * Author: Mark Riddoch
 */
#include <modbus_south.h>
#include <reading.h>
#include <logger.h>

using namespace std;

/**
 * Constructor for the modbus interface for a TCP connection
 */
Modbus::Modbus(const string& ip, const unsigned short port) :
	m_address(ip), m_port(port), m_device(""), m_tcp(true)
{
	m_modbus = modbus_new_tcp(ip.c_str(), port);
#if DEBUG
	modbus_set_debug(m_modbus, true);
#endif
	errno = 0;
	if (modbus_connect(m_modbus) == -1)
	{
		Logger::getLogger()->error("Failed to connect to Modbus TCP server %s", modbus_strerror(errno));
		m_connected = false;
	}
	else
	{
		Logger::getLogger()->info("Modbus TCP connected %s:%d", m_address.c_str(), m_port);
	}
}

/**
 * Constructor for the modbus interface for a serial connection
 */
Modbus::Modbus(const string& device, int baud, char parity, int bits, int stopBits) :
	m_device(device), m_address(""), m_port(0), m_tcp(false)
{
	m_modbus = modbus_new_rtu(device.c_str(), baud, parity, bits, stopBits);
#if DEBUG
	modbus_set_debug(m_modbus, true);
#endif
	if (modbus_connect(m_modbus) == -1)
	{
		Logger::getLogger()->error("Failed to connect to Modbus RTU device:  %s", modbus_strerror(errno));
		m_connected = false;
	}
	m_connected = true;
	Logger::getLogger()->info("Modbus RTU connected to %s",  m_device.c_str());
}
/**
 * Destructor for the modbus interface
 */
Modbus::~Modbus()
{
	for (vector<RegisterMap *>::const_iterator it = m_registers.cbegin();
			it != m_registers.cend(); ++it)
	{
		delete *it;
	}
	for (vector<RegisterMap *>::const_iterator it = m_coils.cbegin();
			it != m_coils.cend(); ++it)
	{
		delete *it;
	}
	for (vector<RegisterMap *>::const_iterator it = m_inputs.cbegin();
			it != m_inputs.cend(); ++it)
	{
		delete *it;
	}
	for (vector<RegisterMap *>::const_iterator it = m_inputRegisters.cbegin();
			it != m_inputRegisters.cend(); ++it)
	{
		delete *it;
	}
	modbus_free(m_modbus);
}

/**
 * Set the slave ID of the modbus node we are interacting with
 *
 * @param slave		The modbus slave ID
 */
void Modbus::setSlave(int slave)
{
	modbus_set_slave(m_modbus, slave);
}

/**
 * Add a registers for a particular slave
 *
 * @param slave		The slave ID we are referencing
 * @param value		The datapoint name for the associated reading
 * @param registerNo	The modbus register number
 */
void Modbus::addRegister(const int slave, const string& value, const unsigned int registerNo, double scale, double offset)
{
	if (m_slaveRegisters.find(slave) != m_slaveRegisters.end())
	{
		m_slaveRegisters[slave].push_back(new Modbus::RegisterMap(value, registerNo, scale, offset));
	}
	else
	{
		vector<Modbus::RegisterMap *> empty;
		m_slaveRegisters.insert(pair<int, vector<Modbus::RegisterMap *> >(slave, empty));
		m_slaveRegisters[slave].push_back(new Modbus::RegisterMap(value, registerNo, scale, offset));
	}
}

/**
 * Add a coil for a particular slave
 *
 * @param slave		The slave ID we are referencing
 * @param value		The datapoint name for the associated reading
 * @param registerNo	The modbus register number
 */
void Modbus::addCoil(const int slave, const string& value, const unsigned int registerNo, double scale, double offset)
{
	if (m_slaveCoils.find(slave) != m_slaveCoils.end())
	{
		m_slaveCoils[slave].push_back(new Modbus::RegisterMap(value, registerNo, scale, offset));
	}
	else
	{
		vector<Modbus::RegisterMap *> empty;
		m_slaveCoils.insert(pair<int, vector<Modbus::RegisterMap *> >(slave, empty));
		m_slaveCoils[slave].push_back(new Modbus::RegisterMap(value, registerNo, scale, offset));
	}
}

/**
 * Add an input for a particular slave
 *
 * @param slave		The slave ID we are referencing
 * @param value		The datapoint name for the associated reading
 * @param registerNo	The modbus register number
 */
void Modbus::addInput(const int slave, const string& value, const unsigned int registerNo, double scale, double offset)
{
	if (m_slaveInputs.find(slave) != m_slaveInputs.end())
	{
		m_slaveInputs[slave].push_back(new Modbus::RegisterMap(value, registerNo, scale, offset));
	}
	else
	{
		vector<Modbus::RegisterMap *> empty;
		m_slaveInputs.insert(pair<int, vector<Modbus::RegisterMap *> >(slave, empty));
		m_slaveInputs[slave].push_back(new Modbus::RegisterMap(value, registerNo, scale, offset));
	}
}

/**
 * Add an input registers for a particular slave
 *
 * @param slave		The slave ID we are referencing
 * @param value		The datapoint name for the associated reading
 * @param registerNo	The modbus register number
 */
void Modbus::addInputRegister(const int slave, const string& value, const unsigned int registerNo, double scale, double offset)
{
	if (m_slaveInputRegisters.find(slave) != m_slaveInputRegisters.end())
	{
		m_slaveInputRegisters[slave].push_back(new Modbus::RegisterMap(value, registerNo, scale, offset));
	}
	else
	{
		vector<Modbus::RegisterMap *> empty;
		m_slaveInputRegisters.insert(pair<int, vector<Modbus::RegisterMap *> >(slave, empty));
		m_slaveInputRegisters[slave].push_back(new Modbus::RegisterMap(value, registerNo, scale, offset));
	}
}


/**
 * Take a reading from the modbus
 */
Reading	Modbus::takeReading()
{
vector<Datapoint *>	points;

	if (!m_connected)
	{
		errno = 0;
		if (modbus_connect(m_modbus) == -1)
		{
			Logger::getLogger()->error("Failed to connect to Modbus device: %s", modbus_strerror(errno));
			return Reading("failed", points);
		}
		m_connected = true;
	}

	/*
	 * First do the readings from the default slave. This is really here to support backward compatibility.
	 */
	setSlave(m_defaultSlave);
	for (int i = 0; i < m_coils.size(); i++)
	{
		uint8_t	coilValue;
		if (modbus_read_bits(m_modbus, m_coils[i]->m_registerNo, 1, &coilValue) == 1)
		{
			DatapointValue value((long)coilValue);
			points.push_back(new Datapoint(m_coils[i]->m_name, value));
		}
		else if (errno == EPIPE)
		{
			m_connected = false;
		}
	}
	for (int i = 0; i < m_inputs.size(); i++)
	{
		uint8_t	inputValue;
		int		rc;
		if (modbus_read_input_bits(m_modbus, m_inputs[i]->m_registerNo, 1, &inputValue) == 1)
		{
			DatapointValue value((long)inputValue);
			points.push_back(new Datapoint(m_inputs[i]->m_name, value));
		}
		else if (rc == -1)
		{
			Logger::getLogger()->error("Modbus read input bits %d, %s", m_inputs[i]->m_registerNo, modbus_strerror(errno));
		}
		else if (errno == EPIPE)
		{
			m_connected = false;
		}
	}
	for (int i = 0; i < m_registers.size(); i++)
	{
		uint16_t	regValue;
		int		rc;
		errno = 0;
		if ((rc = modbus_read_registers(m_modbus, m_registers[i]->m_registerNo, 1, &regValue)) == 1)
		{
			DatapointValue value((long)regValue);
			points.push_back(new Datapoint(m_registers[i]->m_name, value));
		}
		else if (rc == -1)
		{
			Logger::getLogger()->error("Modbus read register %d, %s", m_registers[i]->m_registerNo, modbus_strerror(errno));
		}
		if (errno == EPIPE)
		{
			m_connected = false;
		}
	}
	for (int i = 0; i < m_inputRegisters.size(); i++)
	{
		uint16_t	regValue;
		int		rc;
		errno = 0;
		if ((rc = modbus_read_input_registers(m_modbus, m_inputRegisters[i]->m_registerNo, 1, &regValue)) == 1)
		{
			DatapointValue value((long)regValue);
			points.push_back(new Datapoint(m_inputRegisters[i]->m_name, value));
		}
		else if (rc == -1)
		{
			Logger::getLogger()->error("Modbus read register %d, %s", m_inputRegisters[i]->m_registerNo, modbus_strerror(errno));
		}
		if (errno == EPIPE)
		{
			m_connected = false;
		}
	}
	/*
	 * Now process items defined using the newer flexible configuration mechanism
	 */
	for (auto it = m_slaveCoils.cbegin(); it != m_slaveCoils.cend(); it++)
	{
		setSlave(it->first);
		for (int i = 0; i < it->second.size(); i++)
		{
			uint8_t	coilValue;
			if (modbus_read_bits(m_modbus, it->second[i]->m_registerNo, 1, &coilValue) == 1)
			{
				DatapointValue value((long)coilValue);
				points.push_back(new Datapoint(it->second[i]->m_name, value));
			}
			else if (errno == EPIPE)
			{
				m_connected = false;
			}
		}
	}
	for (auto it = m_slaveInputs.cbegin(); it != m_slaveInputs.cend(); it++)
	{
		setSlave(it->first);
		for (int i = 0; i < it->second.size(); i++)
		{
			uint8_t	inputValue;
			if (modbus_read_input_bits(m_modbus, it->second[i]->m_registerNo, 1, &inputValue) == 1)
			{
				double finalValue = it->second[i]->m_offset + (inputValue * it->second[i]->m_scale);
				DatapointValue value(finalValue);
				points.push_back(new Datapoint(it->second[i]->m_name, value));
			}
			else if (errno == EPIPE)
			{
				m_connected = false;
			}
		}
	}
	for (auto it = m_slaveRegisters.cbegin(); it != m_slaveRegisters.cend(); it++)
	{
		setSlave(it->first);
		for (int i = 0; i < it->second.size(); i++)
		{
			uint16_t	registerValue;
			if (modbus_read_registers(m_modbus, it->second[i]->m_registerNo, 1, &registerValue) == 1)
			{
				double finalValue = it->second[i]->m_offset + (registerValue * it->second[i]->m_scale);
				DatapointValue value(finalValue);
				points.push_back(new Datapoint(it->second[i]->m_name, value));
			}
			else if (errno == EPIPE)
			{
				m_connected = false;
			}
		}
	}
	for (auto it = m_slaveInputRegisters.cbegin(); it != m_slaveInputRegisters.cend(); it++)
	{
		setSlave(it->first);
		for (int i = 0; i < it->second.size(); i++)
		{
			uint16_t	registerValue;
			if (modbus_read_input_registers(m_modbus, it->second[i]->m_registerNo, 1, &registerValue) == 1)
			{
				double finalValue = it->second[i]->m_offset + (registerValue * it->second[i]->m_scale);
				DatapointValue value(finalValue);
				points.push_back(new Datapoint(it->second[i]->m_name, value));
			}
			else if (errno == EPIPE)
			{
				m_connected = false;
			}
		}
	}
	return Reading(m_assetName, points);
}
