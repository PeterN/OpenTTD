/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * OpenTTD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with OpenTTD. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file cargomonitor_sl.cpp Code handling saving and loading of Cargo monitoring. */

#include "../stdafx.h"

#include "saveload.h"
#include "compat/cargomonitor_sl_compat.h"

#include "../cargomonitor.h"

#include "../safeguards.h"

/** Temporary storage of cargo monitoring data for loading or saving it. */
struct TempStorage {
	CargoMonitorID number;
	bool is_industry;
	TownID town;
	IndustryID industry;
	CargoType cargo;
	CompanyID company;
	uint32_t amount;
};

/** Description of the #TempStorage structure for the purpose of load and save. */
static const SaveLoad _cargomonitor_pair_desc[] = {
	SLE_CONDVAR(TempStorage, is_industry, SLE_BOOL, SLV_VARIABLE_CARGO_ARRAY, SL_MAX_VERSION),
	SLE_CONDVAR(TempStorage, town, SLE_UINT16, SLV_VARIABLE_CARGO_ARRAY, SL_MAX_VERSION),
	SLE_CONDVAR(TempStorage, industry, SLE_UINT16, SLV_VARIABLE_CARGO_ARRAY, SL_MAX_VERSION),
	SLE_CONDVAR(TempStorage, cargo, SLE_UINT8, SLV_VARIABLE_CARGO_ARRAY, SL_MAX_VERSION),
	SLE_CONDVAR(TempStorage, company, SLE_UINT8, SLV_VARIABLE_CARGO_ARRAY, SL_MAX_VERSION),
	SLE_CONDVAR(TempStorage, number, SLE_UINT32, SL_MIN_VERSION, SLV_VARIABLE_CARGO_ARRAY),
	SLE_VAR(TempStorage, amount, SLE_UINT32),
};

static CargoMonitorID FixupCargoMonitor(CargoMonitorID number)
{
	/* Between SLV_EXTEND_CARGOTYPES and SLV_FIX_CARGO_MONITOR, the
	 * CargoMonitorID structure had insufficient packing for more
	 * than 32 cargo types. Here we have to shuffle bits to account
	 * for the change.
	 * Company moved from bits 24-31 to 25-28.
	 * Cargo type increased from bits 19-23 to 19-24.
	 */
	SB(number, 25, 4, GB(number, 24, 4));
	SB(number, 29, 3, 0);
	ClrBit(number, 24);
	return number;
}

/** #_cargo_deliveries monitoring map. */
struct CMDLChunkHandler : ChunkHandler {
	CMDLChunkHandler() : ChunkHandler('CMDL', CH_TABLE) {}

	void Save() const override
	{
		SlTableHeader(_cargomonitor_pair_desc);

		TempStorage storage;

		int i = 0;
		CargoMonitorMap::const_iterator iter = _cargo_deliveries.begin();
		while (iter != _cargo_deliveries.end()) {
			storage.is_industry = MonitorMonitorsIndustry(iter->first);
			storage.town = DecodeMonitorTown(iter->first);
			storage.industry = DecodeMonitorIndustry(iter->first);
			storage.cargo = DecodeMonitorCargoType(iter->first);
			storage.company = DecodeMonitorCompany(iter->first);
			storage.amount = iter->second;

			SlSetArrayIndex(i);
			SlObject(&storage, _cargomonitor_pair_desc);

			i++;
			iter++;
		}
	}

	void Load() const override
	{
		const std::vector<SaveLoad> slt = SlCompatTableHeader(_cargomonitor_pair_desc, _cargomonitor_pair_sl_compat);

		TempStorage storage;
		bool fix = IsSavegameVersionBefore(SLV_FIX_CARGO_MONITOR);
		bool decode = IsSavegameVersionBefore(SLV_VARIABLE_CARGO_ARRAY);

		ClearCargoDeliveryMonitoring();
		for (;;) {
			if (SlIterateArray() < 0) break;
			SlObject(&storage, slt);

			if (fix) storage.number = FixupCargoMonitor(storage.number);

			if (decode) {
				/* Bit-fields encoded into the cargo monitor before it was unpacked. */
				constexpr uint8_t OLD_CCB_TOWN_IND_NUMBER_START = 0; ///< Start bit of the town or industry number.
				constexpr uint8_t OLD_CCB_TOWN_IND_NUMBER_LENGTH = 16; ///< Number of bits of the town or industry number.
				constexpr uint8_t OLD_CCB_IS_INDUSTRY_BIT = 16; ///< Bit indicating the town/industry number is an industry.
				constexpr uint8_t OLD_CCB_CARGO_TYPE_START = 19; ///< Start bit of the cargo type field.
				constexpr uint8_t OLD_CCB_CARGO_TYPE_LENGTH = 6; ///< Number of bits of the cargo type field.
				constexpr uint8_t OLD_CCB_COMPANY_START = 25; ///< Start bit of the company field.
				constexpr uint8_t OLD_CCB_COMPANY_LENGTH = 4; ///< Number of bits of the company field.

				storage.is_industry = HasBit(storage.number, OLD_CCB_IS_INDUSTRY_BIT);
				storage.town = storage.is_industry ? INVALID_TOWN : static_cast<TownID>(GB(storage.number, OLD_CCB_TOWN_IND_NUMBER_START, OLD_CCB_TOWN_IND_NUMBER_LENGTH));
				storage.industry = storage.is_industry ? static_cast<IndustryID>(GB(storage.number, OLD_CCB_TOWN_IND_NUMBER_START, OLD_CCB_TOWN_IND_NUMBER_LENGTH)) : INVALID_INDUSTRY;
				storage.cargo = static_cast<CargoType>(GB(storage.number, OLD_CCB_CARGO_TYPE_START, OLD_CCB_CARGO_TYPE_LENGTH));
				storage.company = static_cast<CompanyID>(GB(storage.number, OLD_CCB_COMPANY_START, OLD_CCB_COMPANY_LENGTH));
			}

			if (storage.is_industry) {
				storage.number = EncodeCargoIndustryMonitor(storage.company, storage.cargo, storage.industry);
			} else {
				storage.number = EncodeCargoTownMonitor(storage.company, storage.cargo, storage.town);
			}

			_cargo_deliveries.emplace(storage.number, storage.amount);
		}
	}
};

/** #_cargo_pickups monitoring map. */
struct CMPUChunkHandler : ChunkHandler {
	CMPUChunkHandler() : ChunkHandler('CMPU', CH_TABLE) {}

	void Save() const override
	{
		SlTableHeader(_cargomonitor_pair_desc);

		TempStorage storage;

		int i = 0;
		CargoMonitorMap::const_iterator iter = _cargo_pickups.begin();
		while (iter != _cargo_pickups.end()) {
			storage.number = iter->first;
			storage.amount = iter->second;

			SlSetArrayIndex(i);
			SlObject(&storage, _cargomonitor_pair_desc);

			i++;
			iter++;
		}
	}

	void Load() const override
	{
		const std::vector<SaveLoad> slt = SlCompatTableHeader(_cargomonitor_pair_desc, _cargomonitor_pair_sl_compat);

		TempStorage storage;
		bool fix = IsSavegameVersionBefore(SLV_FIX_CARGO_MONITOR);

		ClearCargoPickupMonitoring();
		for (;;) {
			if (SlIterateArray() < 0) break;
			SlObject(&storage, slt);

			if (fix) storage.number = FixupCargoMonitor(storage.number);

			_cargo_pickups.emplace(storage.number, storage.amount);
		}
	}
};

/** Chunk definition of the cargomonitoring maps. */
static const CMDLChunkHandler CMDL;
static const CMPUChunkHandler CMPU;
static const ChunkHandlerRef cargomonitor_chunk_handlers[] = {
	CMDL,
	CMPU,
};

extern const ChunkHandlerTable _cargomonitor_chunk_handlers(cargomonitor_chunk_handlers);
