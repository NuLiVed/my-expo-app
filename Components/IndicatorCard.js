import React from 'react';
import { View, Text, StyleSheet } from 'react-native';
import { MaterialCommunityIcons, Feather } from '@expo/vector-icons';

export default function IndicatorCard({ type, value, unit, icon, status, optimal, timestamp, trend }) {
  return (
    <View style={styles.card}>
      <View style={styles.topRow}>
        <MaterialCommunityIcons name={icon} size={24} color="#E67E22" />
        <View style={styles.rightTop}>
          <View style={styles.status}>
            <View style={[styles.dot, status === 'Live' && styles.live]} />
            <Text style={styles.statusText}>{status}</Text>
          </View>
          {trend && (
            <View style={styles.trend}>
              <Feather name={trend.direction === 'up' ? 'arrow-up' : 'arrow-down'} size={16} color={trend.color} />
              <Text style={[styles.trendText, { color: trend.color }]}>{trend.value}{unit}</Text>
            </View>
          )}
        </View>
      </View>

      <Text style={styles.value}>{value}{unit}</Text>
      {optimal && <Text style={styles.optimal}>Optimal range: {optimal}</Text>}
      {timestamp && <Text style={styles.timestamp}>Last updated: {timestamp}</Text>}
    </View>
  );
}

const styles = StyleSheet.create({
  card: {
    backgroundColor: '#FFFFFF',
    borderRadius: 10,
    padding: 16,
    margin: 8,
    shadowColor: '#000',
    shadowOffset: { width: 0, height: 2 },
    shadowOpacity: 0.1,
    shadowRadius: 4,
    elevation: 3,
    flex: 1,
  },
  topRow: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'flex-start',
    marginBottom: 8,
  },
  rightTop: {
    alignItems: 'flex-end',
  },
  status: {
    flexDirection: 'row',
    alignItems: 'center',
    marginBottom: 4,
  },
  dot: {
    width: 8,
    height: 8,
    borderRadius: 4,
    backgroundColor: '#ccc',
    marginRight: 4,
  },
  live: {
    backgroundColor: '#4CAF50',
  },
  statusText: {
    fontSize: 12,
    color: '#A0522D',
    fontFamily: 'System',
  },
  value: {
    fontSize: 32,
    fontWeight: 'bold',
    color: '#A0522D',
    fontFamily: 'System',
    marginBottom: 4,
  },
  optimal: {
    fontSize: 12,
    color: '#666',
    fontFamily: 'System',
    marginBottom: 4,
  },
  timestamp: {
    fontSize: 10,
    color: '#999',
    fontFamily: 'System',
  },
  trend: {
    flexDirection: 'row',
    alignItems: 'center',
  },
  trendText: {
    fontSize: 12,
    fontFamily: 'System',
    marginLeft: 2,
  },
});
