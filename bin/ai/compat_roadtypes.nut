/* 1.10 adds road sub types. */
AIRoad._IsRoadTypeAvailable <- AIRoad.IsRoadTypeAvailable;
AIRoad.IsRoadTypeAvailable <- function(road_type)
{
        return AIRoad._IsRoadTypeAvailable(road_type, 0);
}

AIRoad._SetCurrentRoadType <- AIRoad.SetCurrentRoadType;
AIRoad.SetCurrentRoadType <- function(road_type)
{
        return AIRoad._SetCurrentRoadType(road_type, 0);
}

AIRoad._HasRoadType <- AIRoad.HasRoadType;
AIRoad.HasRoadType <- function(tile, road_type)
{
        return AIRoad._HasRoadType(tile, road_type, 0);
}

AIRoad._GetBuildCost <- AIRoad.GetBuildCost;
AIRoad.GetBuildCost <- function(road_type, build_type)
{
        return AIRoad._GetBuildCost(road_type, 0, build_type);
}

AIRoad._GetMaintenanceCostFactor <- AIRoad.GetMaintenanceCostFactor;
AIRoad.GetMaintenanceCostFactor <- function(road_type)
{
        return AIRoad._GetMaintenanceCostFactor(road_type, 0);
}

AIInfrastructure._GetRoadPieceCount <- AIInfrastructure.GetRoadPieceCount;
AIInfrastructure.GetRoadPieceCount <- function(road_type)
{
	return AIInfrastructure._GetRoadPieceCount(road_type, 0);
}

