/* 1.9 adds road sub types. */
GSRoad._IsRoadTypeAvailable <- GSRoad.IsRoadTypeAvailable;
GSRoad.IsRoadTypeAvailable <- function(road_type)
{
        return GSRoad._IsRoadTypeAvailable(road_type, 0);
}

GSRoad._SetCurrentRoadType <- GSRoad.SetCurrentRoadType;
GSRoad.SetCurrentRoadType <- function(road_type)
{
        return GSRoad._SetCurrentRoadType(road_type, 0);
}

GSRoad._HasRoadType <- GSRoad.HasRoadType;
GSRoad.HasRoadType <- function(tile, road_type)
{
        return GSRoad._HasRoadType(tile, road_type, 0);
}

GSRoad._GetBuildCost <- GSRoad.GetBuildCost;
GSRoad.GetBuildCost <- function(road_type, build_type)
{
        return GSRoad._GetBuildCost(road_type, 0, build_type);
}

GSRoad._GetMaintenanceCostFactor <- GSRoad.GetMaintenanceCostFactor;
GSRoad.GetMaintenanceCostFactor <- function(road_type)
{
        return GSRoad._GetMaintenanceCostFactor(road_type, 0);
}

GSInfrastructure._GetRoadPieceCount <- GSInfrastructure.GetRoadPieceCount;
GSInfrastructure.GetRoadPieceCount <- function(road_type)
{
        return GSInfrastructure._GetRoadPieceCount(road_type, 0);
}
